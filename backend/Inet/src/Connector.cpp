
#include "Logger.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "SocketsOps.hpp"
#include "Connector.hpp"

#include <memory>

#include<string.h>
#include <errno.h>

namespace shanchuan
{
    // class Channel;
    // class EventLoop;
    // class Connector
    // using NewConnectionCallback = std::function<void(int sockfd)>;
    //   enum class States  { kDisconnected, kConnection, kConnected };
    //  EventLoop *loop_;
    //  shanchuan::InetAddress serverAddr_;
    // bool connect_;
    // States state_;
    // std::unique_ptr<Channel> channel_;
    //  NewConnectionCallback newConnectionCallback_;
    // int retryDelayMs_;
    // static const int kMaxRetryDelayMs = 30 * 1000; // 最大重试延迟
    //  static const int kInitRetryDelayMs = 50;       // 初始化重试延迟

    const int Connector::kMaxRetryDelayMs = 30 * 1000; // 最大重试延迟
    const int Connector::kInitRetryDelayMs = 500;      // 初始化重试延迟
    void Connector::startInLoop()
    {
        LOG_TRACE<<"startInLoop connect_"<<connect_;
        loop_->assertInLoopThread();
        assert(state_ == States::kDisconnected);
        LOG_TRACE<<connect_;
        if (connect_)
        {
            connect();
        }
        else
        {
            LOG_DEBUG << "不连接do not connect";
        }
        LOG_TRACE<<"startInLoop connect_"<<connect_;
    }
    void Connector::stopInLoop()
    {
        LOG_TRACE<<"BEGIN";
        loop_->assertInLoopThread();
        if (state_ == States::kConnecting)
        {
            setState(States::kDisconnected);
            int sockfd = removeAndResetChannel();
            retry(sockfd);
        }
    }
    void Connector::connect()
    {
        LOG_TRACE<<"BEGIN";
        int sockfd = Sockets::createNonblockingOrDie();
        int ret = Sockets::Connect(sockfd, serverAddr_.getSockAddrInet());
        int saveErrno = (ret == 0) ? 0 : errno;
        LOG_TRACE<<"ret connect: errno "<<saveErrno<<" "<<strerror(saveErrno);
        switch (saveErrno)
        {
        case 0:
        case EINPROGRESS: //操作正在进行中
        case EINTR:
        case EISCONN:
            LOG_TRACE<<"call connection: "<<sockfd;
            connecting(sockfd);
            break;
        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            LOG_TRACE<<"call retry : "<<sockfd;
            retry(sockfd);
            break;
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_SYSERR << "connect error in Connector::startInLoop" << saveErrno;
            Sockets::close(sockfd);
            break;
        default:
            LOG_SYSERR << "unepected error in Connector::startInLoop" << saveErrno;
            Sockets::close(sockfd);
            break;
        }
        LOG_TRACE<<"END";
    }
    void Connector::connecting(int sockfd)
    {
        LOG_TRACE<<"connecting: "<<StatesToString();
        setState(States::kConnecting);
        assert(!channel_);
        channel_.reset(new Channel(loop_,sockfd));
        channel_->setWriteCallback(std::bind(&Connector::handleWrite,this));
        channel_->setErrorCallback(std::bind(&Connector::handleError,this));
        channel_->enableWriting();
        LOG_TRACE<<"connecting: "<<StatesToString();
    }
    void Connector::handleWrite()
    {
        LOG_TRACE<<"Connector::handleWrite"<<StatesToString();
        if(States::kConnecting == state_)
        {
            int sockfd = removeAndResetChannel();
            int err = Sockets::getSocketError(sockfd);
            if(err)
            {
                LOG_WARN<<"Connector::handleWrite - SO_ERROR = "
                        << err<<" "<< strerror(err)
                        << err << " "<<strerror(err);

                retry(sockfd);
            }
            else if(Sockets::isSelfConnect(sockfd))
            {
                LOG_WARN<<"Connector::handleWrite - Self connect";
                retry(sockfd);
            }
            else
            {
                setState(States::kConnected);
                LOG_TRACE<<"setState(States::kConnected)  connect_"<<connect_;
                if(connect_)
                {
                    newConnectionCallback_(sockfd);
                }
                else
                {
                    Sockets::close(sockfd);
                }
            }
        }
        else
        {
            assert(state_ == States::kDisconnected);
        }
        LOG_TRACE<<"END";
    }
    void Connector::handleError()
    {
        LOG_ERROR<<"Connector::handleError state = "<<StatesToString();
        if(state_ == States::kConnecting)
        {
            int sockfd = removeAndResetChannel();
            int err = Sockets::getSocketError(sockfd);
            LOG_TRACE<<"SO_ERROR = "<<err<<" "<<strerror(err);
            retry(sockfd);
        }
    }
    void Connector::retry(int sockfd)
    {
        LOG_TRACE<<"begin";
        Sockets::close(sockfd);
        setState(States::kDisconnected);
        if(connect_)
        {
            LOG_INFO<<"Connector::retry - Retry connection to "<<serverAddr_.toIpPort()
                    << " in "<<retryDelayMs_<<" milliseconds. ";
            loop_->runAfter(retryDelayMs_ / 1000.0,
                            std::bind(&Connector::startInLoop,shared_from_this()));
            retryDelayMs_ = std::min(retryDelayMs_ *2 , kMaxRetryDelayMs);
        }
        else
        {
            LOG_DEBUG<<"do not connect";
        }
    }
    int Connector::removeAndResetChannel()
    {
        LOG_TRACE<<"start";
        channel_->disableAll();
        channel_->remove();
        int sockfd = channel_->fd();
        loop_->queueInLoop(std::bind(&Connector::resetChannel,this));
        LOG_TRACE<<"END ";
        return sockfd;
    }
    void Connector::resetChannel()
    {
        LOG_TRACE<<"BEGIN";
        channel_.reset();
    }

    Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
        : loop_(loop),
          serverAddr_(serverAddr),
          connect_(false),
          state_(States::kDisconnected),
          retryDelayMs_(kInitRetryDelayMs) 
    {
       LOG_DEBUG << "ctor [ " << this << "]";
    }
    Connector::~Connector()
    {
        LOG_DEBUG << "dtor [ " << (size_t)this << "]";
    }
    void Connector::start()
    {
        LOG_TRACE<<"connect_: "<<connect_;
        connect_ = true;
        loop_->runInLoop(std::bind(&Connector::startInLoop, this));
        LOG_TRACE<<"connect_: "<<connect_;
    }
    void Connector::restart()
    {
        LOG_TRACE<<"BEGIN";
        
        loop_->assertInLoopThread();
        setState(States::kDisconnected);
        retryDelayMs_ = kInitRetryDelayMs;
        connect_ = true;
        startInLoop();
    }
    void Connector::stop()
    {
        LOG_TRACE<<"BEGIN";
        connect_ = false;
        loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
    }

} // namespace shanchuan

/*
1. EINPROGRESS
含义：该错误码通常在非阻塞模式下使用 connect() 系统调用时出现。
当调用 connect() 发起一个 TCP 连接时，如果连接不能立即建立完成（例如，目标服务器可能暂时不可达），
在非阻塞套接字上 connect() 会立即返回，并且将 errno 设置为 EINPROGRESS，
表示连接操作正在进行中。

2. EINTR
含义：该错误码表示系统调用被信号中断。当一个进程在执行某个系统调用时，如果收到一个信号，
并且该信号的处理函数被调用，那么正在执行的系统调用可能会被中断，此时系统调用会返回 -1，
并且将 errno 设置为 EINTR。

3. EISCONN
含义：该错误码表示套接字已经连接。当你尝试对一个已经连接的套接字再次调用 connect() 时，
connect() 会失败，并且将 errno 设置为 EISCONN。

1. EAGAIN
含义：
在非阻塞 I/O 操作中，当操作无法立即完成时，系统调用会返回 -1 并将 errno 设置为
 EAGAIN（在某些系统上也可能是 EWOULDBLOCK，二者等价）。
 例如，当从一个非阻塞套接字读取数据时，如果没有数据可读，read() 调用就会返回 EAGAIN， 表示稍后再试。
对于某些资源（如线程池中的线程、信号量等）耗尽时，获取资源的操作也可能返回 EAGAIN，提示当前没有可用资源，
需要稍后重试。

2. EADDRINUSE
含义：当使用 bind() 系统调用将套接字绑定到一个地址和端口时，如果该地址和端口已经被其他套接字占用，
bind() 会失败并将 errno 设置为 EADDRINUSE。这通常意味着另一个程序已经在使用该地址和端口进行监听或通信。

3. EADDRNOTAVAIL
含义：当使用 bind() 或 connect() 系统调用时，如果指定的地址不可用，系统调用会失败并将 errno 设置为
 EADDRNOTAVAIL。
 这可能是因为指定的 IP 地址不在本地机器的网络接口上，或者指定的地址是一个广播地址但没有设置相应的选项。

 4. ECONNREFUSED
含义：当使用 connect() 系统调用尝试连接到一个远程服务器时，如果目标服务器拒绝连接，
connect() 会失败并将 errno 设置为 ECONNREFUSED。
这通常是因为目标端口没有监听程序，或者服务器拒绝接受新的连接。

5. ENETUNREACH
含义：当使用 connect() 或其他网络相关的系统调用时，如果本地主机无法到达目标网络，
系统调用会失败并将 errno 设置为 ENETUNREACH。这可能是因为网络接口故障、路由表错误或目标网络不可达。

1. EACCES
含义：表示没有足够的权限执行该操作。在网络编程中，可能是尝试绑定一个特权端口（通常是 1 - 1023），
但当前进程没有 root 权限。

2. EPERM
含义：同样表示权限问题，但与 EACCES 稍有不同，EPERM 通常意味着操作违反了系统的安全策略或权限设置。
例如，尝试在没有权限的情况下更改套接字的某些选项。

3. EAFNOSUPPORT
含义：表示指定的地址族（Address Family）不被支持。例如，尝试使用一个不支持的协议族创建套接字，
或者在 connect() 或 bind() 时使用了不支持的地址族。

4. EALREADY
含义：通常在非阻塞模式下，当已经有一个异步连接操作正在进行时，再次尝试发起连接会返回 EALREADY。
这意味着前一个连接操作还未完成。

5. EBADF
含义：表示传递给系统调用的文件描述符无效。在网络编程中，可能是使用了一个已经关闭的套接字描述符，
或者传递了一个非套接字的文件描述符。

6. EFAULT
含义：表示传递给系统调用的指针参数指向了无效的内存地址。
例如，在 send() 或 recv() 时传递了一个无效的缓冲区指针。

7. ENOTSOCK
含义：表示传递给系统调用的文件描述符不是一个有效的套接字描述符。
例如，尝试对一个普通文件的文件描述符使用套接字相关的操作。


*/