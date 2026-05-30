
/**
 * Reactor模式， 每个线程最多一个EventLoop (One loop per thread).
 * 接口类, 不要暴露太多细节给客户
 */
// C++ STL
#include <functional>
#include <memory>
#include <algorithm>
using namespace std;
// Liunx API
#include "signal.h"
#include "sys/eventfd.h"
#include <unistd.h>

// OWNER
#include "Logger.hpp"
#include "Channel.hpp"
#include "Poller.hpp"
#include "SocketsOps.hpp"
#include "EventLoop.hpp"

namespace shanchuan
{
    __thread EventLoop *t_loopInThisThread = nullptr;

    const int kPollTimeMs = 10000;

    int createEventfd()
    {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0)
        {
            LOG_SYSERR << "Failed in eventfd";
            abort();
        }
        return evtfd;
    }
    class IgnoreSigPipe
    {
        public:
        IgnoreSigPipe()
        {
            ::signal(SIGPIPE,SIG_IGN);
        }
    };
    IgnoreSigPipe initObj;


    EventLoop *EventLoop::getEventLoopOfCurrentThread()
    {
        return t_loopInThisThread;
    }

    EventLoop::EventLoop()
        : looping_(false), // 表示还未循环
          quit_(false),
          eventHandling_(false),
          callingPendingFunctors_(false),
          iteration_(0),
          threadId_(std::this_thread::get_id()),   // 赋值线程id
          poller_(Poller::newDefaultPoller(this)), // 构造了一个实际的poller对象
          timerQueue_(new TimerQueue(this)),
          wakeupFd_(createEventfd()),
          wakeupChannel_(new Channel(this,wakeupFd_)),
          currentActiveChannel_(NULL)
    {
        LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
        if (t_loopInThisThread)
        {
            LOG_FATAL << "Another EventLoop " << t_loopInThisThread
                      << " exists in this thread " << threadId_;
        }
        else
        {
            t_loopInThisThread = this;
        }
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
        wakeupChannel_->enableReading();
    }
    EventLoop::~EventLoop()
    {
        LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
                  << " destructs in thread " << std::this_thread::get_id();
        wakeupChannel_->disableAll();
        wakeupChannel_->remove();
        close(wakeupFd_);
        t_loopInThisThread = nullptr;
    }

    void EventLoop::runInLoop(const Functor &cb)
    {
        if (isInLoopThread())
        {
            LOG_TRACE<<"EventLoop::runInLoop"<<"在当前线程中";
            cb();
        }
        else
        {
            LOG_TRACE<<"EventLoop::runInLoop"<<"不在当前线程中 call queueInLoop";
            queueInLoop(std::move(cb));
        }
    }
    void EventLoop::loop()
    {
        assert(!looping_);
        assertInLoopThread(); // 断言处于创建该对象的线程中
        looping_ = true;
        quit_ = false; // FIXME: what if someone calls quit() before loop() ?

        LOG_TRACE << "EventLoop " << this << " start looping";
        while (!quit_)
        {
            activeChannels_.clear();
            // 调用poll返回活动的通道，//有可能是唤醒返回的
            pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
            ++iteration_;
            LOG_TRACE << "poller->poll activeChannels_.size(): " << activeChannels_.size();
            //if (Logger::getLogLevel() <= LOG_LEVEL::TRACE)
            {
                printActiveChannels(); // 日志登记，日志打印
            }
            // TODO sort channel by priority
            eventHandling_ = true;
            // 遍历通道来进行处理
            for (auto it : activeChannels_)
            {
                LOG_TRACE << it->getEventHandling();
                currentActiveChannel_ = it;
                it->handleEvent(pollReturnTime_);
                LOG_TRACE << it->getEventHandling();
            }
            currentActiveChannel_ = NULL;
            eventHandling_ = false;
            // I/O线程设计比较灵活，通过下面这个设计也能够进行计算任务，否则当I/O不是很繁忙的时候，
            // 这个I/O线程就一直处于阻塞状态。我们需要让它也能执行一些计算任务
            doPendingFunctors();
        }
        LOG_TRACE << "EventLoop " << this << " stop looping";
        looping_ = false;
    }
    void EventLoop::quit()
    {
        LOG_TRACE<<"quit: "<<quit_;
        quit_ = true;
        if(!isInLoopThread())
        {
            wakeup();
        }
    }

    void EventLoop::updateChannel(Channel *channel)
    {
        LOG_TRACE<<"(Channel *channel)";
        assert(channel->ownerLoop() == this);
        assertInLoopThread();
        poller_->updateChannel(channel);
    }

    void EventLoop::removeChannel(Channel *channel)
    {
        LOG_TRACE<<"BEGIN";
        assert(channel->ownerLoop() == this);
        assertInLoopThread();
        LOG_TRACE<<"eventHandling : "<<eventHandling_;
        if (eventHandling_)
        {
            assert(currentActiveChannel_ == channel ||
                   std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
        }
        poller_->removeChannel(channel);
    }
    bool EventLoop::hasChannel(Channel *channel)
    {
        assert(channel->ownerLoop() == this);
        assertInLoopThread();
        return poller_->hasChannel(channel);
    }

    void EventLoop::abortNotInLoopThread()
    {
        LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
                  << " was created in threadId_ = " << threadId_
                  << ", current thread id = " << std::this_thread::get_id();
    }
    void EventLoop::wakeup()
    {
        LOG_TRACE<<"in function";
        uint64_t one = 1;
        ssize_t n = Sockets::write(wakeupFd_,&one,sizeof(one));
        if(n != sizeof(one))
        {
            LOG_ERROR<<"EventLoop::wakeup() writes "<<n<<" bytes instead of 8";
        }
        LOG_TRACE<<" end function";
    }
    void EventLoop::handleRead()
    {
        LOG_TRACE<<"HandleRead";
        uint64_t one = 1;
        ssize_t n = Sockets::read(wakeupFd_,&one,sizeof(one));
        if(n != sizeof(one))
        {
            LOG_ERROR<<"EventLoop::handleRead() reads "<<n<<"bytes instead of 8";
        }
    }
    void EventLoop::doPendingFunctors()
    {
        LOG_TRACE<<"EventLoop::doPendingFunctors";
        LOG_TRACE<<"pendingFunctors_.size(): "<<pendingFunctors_.size();
        std::vector<Functor> functors;
        callingPendingFunctors_ = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            functors.swap(pendingFunctors_);
        }
        for(const Functor & functor: functors)
        {
            functor();
        }
        callingPendingFunctors_ = false;
        LOG_TRACE<<"END";
    }
    void EventLoop::printActiveChannels() const
    {
        LOG_TRACE <<"in";
        for (ChannelList::const_iterator it = activeChannels_.begin();
             it != activeChannels_.end(); ++it)
        {
            const Channel *ch = *it;
            LOG_TRACE << "{" << ch->reventsToString() << "} ";
        }
    }
    void EventLoop::queueInLoop(const Functor &cb)
    {
        LOG_TRACE << "EventLoop::queueInLoop";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingFunctors_.push_back(std::move(cb));
            LOG_TRACE<<"pendingFunctors_.push_back(std::move(cb))";
        }
        LOG_TRACE<<"isInLoopThread() "<<isInLoopThread();
        // callingPendingFunctors_  => 调用待定函数_
        LOG_TRACE<<"callingPendingFunctors_ "<<callingPendingFunctors_;
        if(!isInLoopThread() || callingPendingFunctors_)
        {
            LOG_TRACE<<"(!isInLoopThread() || callingPendingFunctors_): call wakeup()";
            wakeup();
        }
    }
    size_t EventLoop::queueSize() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pendingFunctors_.size();
    }

    //在时间戳为time的时间执行，0.0表示一次性不重复
    TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
    {
        LOG_TRACE << "EventLoop::runAt";
        return timerQueue_->addTimer(std::move(cb),time,0.0);
        //return TimerId();
    }
    ////延迟delay时间执行的定时器
    TimerId EventLoop::runAfter(double delay, TimerCallback cb)
    {
        LOG_TRACE << "EventLoop::runAfter";
        shanchuan::Timestamp time(addTime(Timestamp::Now(), delay));
        return runAt(time,std::move(cb));
    }
    ////间隔性的定时器，起始就是重复定时器，间隔interval需要大于0
    TimerId EventLoop::runEvery(double interval, TimerCallback cb)
    {
        LOG_TRACE << "EventLoop::runEvery";
        shanchuan::Timestamp time(addTime(Timestamp::Now(),interval));
        return timerQueue_->addTimer(std::move(cb),time,interval);
    }
    void EventLoop::cancel(TimerId timerId)
    {
        LOG_TRACE<<"timerId: ";
        return timerQueue_->cancel(timerId);
    }

} // namespace shanchuan




/*

代码功能概述
这段代码调用了 eventfd 系统调用，用于创建一个事件文件描述符（event file descriptor）。事件文件描述符是一种特殊的文件描述符，可用于在不同线程或进程之间进行事件通知和同步。
详细解释
1. eventfd 函数原型
eventfd 函数的原型如下：
c
#include <sys/eventfd.h>

int eventfd(unsigned int initval, int flags);
参数说明：
initval：指定事件文件描述符的初始值，这是一个无符号 64 位整数。在这个代码中，初始值被设置为 0。
flags：一组标志，用于指定 eventfd 的行为。在这个代码中，使用了 EFD_NONBLOCK 和 EFD_CLOEXEC 两个标志。
2. 标志解释
EFD_NONBLOCK：
当设置了这个标志时，eventfd 的读和写操作将以非阻塞模式进行。如果没有数据可读，读操作将立即返回 -1 并设置 errno 为 EAGAIN 或 EWOULDBLOCK；如果缓冲区已满，写操作也将立即返回 -1 并设置 errno 为 EAGAIN 或 EWOULDBLOCK。
EFD_CLOEXEC：
当设置了这个标志时，在调用 execve 系列函数执行新程序时，该事件文件描述符将自动关闭。这可以避免在子进程中意外地保留不必要的文件描述符。
3. 代码返回值
eventfd 函数调用成功时，返回一个新的文件描述符，这个文件描述符将被赋值给变量 evtfd。
调用失败时，返回 -1，并设置 errno 来指示具体的错误原因。常见的错误包括 EMFILE（进程的文件描述符表已满）和 ENFILE（系统的文件表已满）等。


*/