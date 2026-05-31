// Liunx API
#include <poll.h>
#include <sys/epoll.h>
// C++ STL
#include <sstream>
using namespace std;
// OWNER
#include "Logger.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
namespace shanchuan
{
    // 无事件
    const int Channel::kNoneEvent = 0;
    // 可读事件
    const int Channel::kReadEvent = POLLIN | POLLPRI;
    // 可写事件
    const int Channel::kWriteEvent = POLLOUT;
    Channel::Channel(EventLoop *loop, int fd)
        : loop_(loop), // channel所属的loop
          fd_(fd),     // channel负责的文件描述符
          events_(0),  // 注册的事件
          revents_(0), // poller设置的就绪的事件
          index_(-1),  // 被poller使用的下标
          tied_(false),
          eventHandling_(false), // 是否处于处理事件中
          addedToLoop_(false)
    {
        LOG_TRACE << "Create Channel ...";
    }
    Channel::~Channel()
    {
        assert(!eventHandling_);
        assert(!addedToLoop_);
        // 移除 loop_->hasChannel(this) 断言
        // 因为这个断言要求在特定线程中析构，导致多线程环境下的问题
        // 如果 addedToLoop_ 为 false，说明 Channel 已经从 loop 中移除，不需要再检查
        LOG_TRACE << "~Channel";
    }
    void Channel::tie(const std::shared_ptr<void> &obj)
    {
        tie_ = obj;
        tied_ = true;
    }

    void Channel::update()
    {
        LOG_TRACE<<"BEGIN";
        addedToLoop_ = true;
        // 确保在正确的 EventLoop 线程中更新 Channel
        loop_->assertInLoopThread();
        loop_->updateChannel(this);
    }
    void Channel::remove()
    {
        assert(isNoneEvent());
        addedToLoop_ = false;
        // 确保在正确的 EventLoop 线程中移除 Channel
        loop_->assertInLoopThread();
        loop_->removeChannel(this);
    }

    //fd得到poller通知以后，处理事件的
    void Channel::handleEvent(Timestamp receiveTime)
    {
        std::shared_ptr<void> guard; //看守;
        LOG_TRACE<<"Channel::handleEvent tied_: "<<tied_;
        if(tied_)  //上层TcpConnection对象还在
        {
            guard = tie_.lock();
            handleEventWithGuard(receiveTime);
        }
        else
        {
            handleEventWithGuard(receiveTime);
        }
    }

    void Channel::handleEventWithGuard(Timestamp receiveTime)
    {
        eventHandling_ = true;
        LOG_TRACE << reventsToString();
        if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
        {
            // if (logHup_)
            LOG_WARN << "Channel::handle_event() POLLHUP "<<" closeCallback_";
            if (closeCallback_)
                closeCallback_();
        }
        if (revents_ & POLLNVAL)
        {
            LOG_WARN << "Channel::handle_event() POLLNVAL";
        }
        if (revents_ & (POLLERR | POLLNVAL))
        {
            LOG_TRACE<<"revents_ & (POLLERR | POLLNVAL) "<<"errorCallback_()";
            if (errorCallback_)
                errorCallback_();
        }
        if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
        {
            LOG_TRACE<<"revents_ & (POLLIN | POLLPRI | POLLRDHUP) "<<"readCallback_()";
            if (readCallback_)
                readCallback_(receiveTime);
        }
        if (revents_ & POLLOUT)
        {
            LOG_TRACE<<"revents_ & POLLOUT) "<<"writeCallback_()";
            if (writeCallback_)
                writeCallback_();
        }
        LOG_TRACE <<" Channel::handleEventWithGuard ";
        eventHandling_ = false;
    }
    string Channel::reventsToString() const
    {
        std::ostringstream oss;
        oss << fd_ << ": ";
        if (revents_ & POLLIN)
            oss << "IN ";
        if (revents_ & POLLPRI)
            oss << "PRI ";
        if (revents_ & POLLOUT)
            oss << "OUT ";
        if (revents_ & POLLHUP)
            oss << "HUP ";
        if (revents_ & POLLRDHUP)
            oss << "RDHUP ";
        if (revents_ & POLLERR)
            oss << "ERR ";
        if (revents_ & POLLNVAL)
            oss << "NVAL ";
        return oss.str().c_str();
    }
}

/*
1.POLLIN
含义：表示文件描述符有数据可读。对于普通文件描述符，意味着有新的数据到达；
对于套接字描述符，可能是有新的连接请求（监听套接字）或者有新的数据可以接收（已连接套接字）。
2. POLLPRI
含义：表示有紧急数据可读。在套接字编程中，通常用于处理带外数据（Out-of-Band Data），
比如 TCP 的紧急指针机制。
ssize_t n = recv(sockfd, buffer, sizeof(buffer), MSG_OOB);
3. POLLOUT
含义：表示文件描述符可写。当文件描述符的发送缓冲区有足够的空间时，该标志会被设置。
对于套接字描述符，意味着可以安全地发送数据而不会阻塞。
4. POLLHUP
含义：表示文件描述符发生了挂起事件。对于套接字描述符，通常意味着对方关闭了连接。
当该标志被设置时，文件描述符将不再可用。
5. POLLRDHUP
含义：表示对端正常关闭连接或者半关闭连接（关闭了写端）。这个标志是在较新的 Linux 内核中引入的，
用于更准确地处理 TCP 连接的关闭事件。
6. POLLERR
含义：表示文件描述符发生了错误。对于套接字描述符，可能是连接失败、网络错误等。
当该标志被设置时，需要检查具体的错误原因。
7. POLLNVAL
含义：表示文件描述符无效。通常是因为传递给 poll 函数的文件描述符没有被正确打开或者已经被关闭。


*/
















