

#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>

#include"Logger.hpp"
#include"Channel.hpp"
#include"EPollPoller.hpp"

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
static_assert(EPOLLIN == POLLIN);
static_assert(EPOLLPRI == POLLPRI);
static_assert(EPOLLOUT == POLLOUT);
static_assert(EPOLLRDHUP == POLLRDHUP);
static_assert(EPOLLERR == POLLERR);
static_assert(EPOLLHUP == POLLHUP);

namespace shanchuan 
{

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    //if (::getenv("USE_POLL"))
    //{
    //    return new PollPoller(loop);
   // }
   // else
   // {
        return new EPollPoller(loop);
   // }
}
static const int kNew = -1;    // Channel 要添加到map和epoll红黑树中
static const int kAdded = 1;   // Channel 在map 和 epoll 的红黑树中
static const int kDeleted = 2; // Channel 在map中，但是不在epoll 的红黑树中

EPollPoller::EPollPoller(EventLoop* loop)
    :Poller(loop),
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize)                            
{
    LOG_TRACE<<"Create ";
    if (epollfd_ < 0)
    {
        LOG_SYSFATAL << "EPollPoller::EPollPoller";
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //int numEvents = ::epoll_wait(epollfd_,
    //                           &*events_.begin(),
    //                           static_cast<int>(events_.size()),
    //                           timeoutMs);

#ifndef NDEBUG
    for(auto &x : channels_)
    {
        LOG_DEBUG<<x.first;
    }
#endif

    int numEvents = ::epoll_wait(epollfd_,
                                events_.data(),
                                events_.size(),
                                timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::Now());
    if (numEvents > 0)
    {
        LOG_TRACE << numEvents << " events happended";
        fillActiveChannels(numEvents, activeChannels);
        if (static_cast<size_t>(numEvents) == events_.size())
        {
            events_.resize(events_.size()*2);
        }
    }else if (numEvents == 0)
    {
        LOG_TRACE << " nothing happended";
    }
    else
    {
        // error happens, log uncommon ones
        if (savedErrno != EINTR)
        {
            errno = savedErrno;
            LOG_SYSERR << "EPollPoller::poll()";
        }
    }
    return now;
}

void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
    assert(static_cast<size_t>(numEvents) <= events_.size());
    for (int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
#endif
        channel->set_revents(events_[i].events); //置每个管道注册的事件
        activeChannels->push_back(channel);      // 把管道事件添加到管道集合中去
    }
}

//根据Channel::index和Channel::isNoneEvent()决定对epoll的文件描述符列表执行添加/删除/更新操作。
//即如果index为kNeworkDeleted，则添加
//不然，如果isNoneEvent()==true,则删除
//如果都不是，就修改
void EPollPoller::updateChannel(Channel* channel)
{
    Poller::assertInLoopThread();
    LOG_TRACE << "fd = " << channel->fd() << " events = "<< channel->events();
    const int index = channel->index();
    LOG_TRACE<<"fd = "<<channel->fd()<<" index: "<<channel->indextoString();
    if (index == kNew || index == kDeleted)
    {
        LOG_TRACE <<" (index == kNew || index == kDeleted)";
        // a new one, add with EPOLL_CTL_ADD
        int fd = channel->fd();
        if (index == kNew)
        {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        }
        else // index == kDeleted
        {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // kAdded
    {
    // update existing one with EPOLL_CTL_MOD/DEL
        LOG_TRACE <<"(index == kAdded)";
        int fd = channel->fd();
        (void)fd;
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);
        if (channel->isNoneEvent())
        {
            LOG_TRACE<<"fd = "<<fd<<"update EPOLL_CTL_DEL";
            update(EPOLL_CTL_DEL, channel); // 
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
    LOG_TRACE<<"END";
}
                        

//移除对channel的监视。
//该函数与updateChannel执行删除的区别是，本函数会删除channels_中对应channel的表项，
//而后者只是删除epoll的文件描述符列表的对应项。
void EPollPoller::removeChannel(Channel* channel)
{
    LOG_TRACE<<"BEGIN";
    Poller::assertInLoopThread();
    int fd = channel->fd();
    LOG_TRACE << "fd = " << fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());
    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    LOG_TRACE<<"index: "<<index;
    size_t n = channels_.erase(fd);    
    (void)n;
    assert(n == 1);
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }                                                        
    channel->set_index(kNew);  
    LOG_TRACE<<"END";                                                                                                         
}
//update将事件添加到红黑树中
void EPollPoller::update(int operation, Channel* channel)
{
    LOG_TRACE<<"BEGIN";
    struct epoll_event event;
    bzero(&event, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_SYSERR << "epoll_ctl op=" << operation << " fd= " << fd;
        }
        else
        {
            LOG_SYSFATAL << "epoll_ctl op=" << operation << " fd= " << fd;
        }
    }
    LOG_TRACE<<"END";
}
}