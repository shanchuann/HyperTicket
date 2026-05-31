
// C++ STL
#include <string>
#include <functional>
using namespace std;

// C
#include <assert.h>
#include <string.h>
// own
#include "Logger.hpp"
#include "Socket.hpp"
#include "SocketsOps.hpp"
#include "TcpConnection.hpp"
#include "WeakCallback.hpp"

namespace shanchuan
{
    void defaultConnectionCallback(const TcpConnectionPtr &conn)
    {
        LOG_TRACE << conn->localAddress().toIpPort() << " -> "
                  << conn->peerAddress().toIpPort() << " is "
                  << (conn->connected() ? "UP" : "DOWN");
    }

    void defaultMessageCallback(const TcpConnectionPtr &,
                                Buffer *buf,
                                Timestamp)
    {
        buf->retrieveAll();
    }
    /*
        TcpConnection::TcpConnection(const std::string &nameArg,
                                     int sockfd,
                                     const InetAddress &localAddr,
                                     const InetAddress &peerAddr)
            : name_(nameArg),
              state_(StateE::kConnecting),
              socket_(new Socket(sockfd)),
              localAddr_(localAddr),
              peerAddr_(peerAddr)
        {
        }
        */

    TcpConnection::TcpConnection(EventLoop *loop,
                                 const string &nameArg,
                                 int sockfd,
                                 const InetAddress &localAddr,
                                 const InetAddress &peerAddr)
        : loop_((assert(loop != nullptr), loop)),
          name_(nameArg),
          state_(StateE::kConnecting),
          socket_(new Socket(sockfd)),
          channel_(new Channel(loop, sockfd)),
          localAddr_(localAddr),
          peerAddr_(peerAddr),
          highWaterMark_(1024 * 1024 * 64)
    {
        LOG_TRACE<<"Create:";
        channel_->setReadCallback( // 读回调
            std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
        channel_->setWriteCallback( // 写回调
            std::bind(&TcpConnection::handleWrite, this));
        channel_->setCloseCallback( // sockfd关闭回调
            std::bind(&TcpConnection::handleClose, this));
        channel_->setErrorCallback( // 错误处理回调
            std::bind(&TcpConnection::handleError, this));
        LOG_DEBUG << "TcpConnection::ctor[" << name_ << "] at " << this
                  << " fd=" << sockfd;
        // socket_->setKeepAlive(true);
        // 设置保持活动状态
        socket_->setKeepAlive(true); // 长连接
    }
    TcpConnection::~TcpConnection()
    {
        LOG_TRACE << "TcpConnection::dtor[" << name_;
        assert(StateE::kDisconnected == state_);
    }
    bool TcpConnection::getTcpInfo(struct tcp_info *tcpi) const
    {
        LOG_TRACE<<"BEGIN";
        return socket_->getTcpInfo(tcpi);
    }
    std::string TcpConnection::getTcpInfoString() const
    {
        LOG_TRACE<<"BEGIN";
        const int len = 1024;
        char buf[len] = {'\0'};
        socket_->getTcpInfoString(buf, sizeof(buf));
        return std::string(buf);
    }

    //send()的返回类型是void,意味着用户不必关心调用send()时成功发送了多少字节，
    //muduo库会保证把数据发送给对方。

    void TcpConnection::send(const void *data, int len)
    {
        LOG_TRACE<<"BEGIN";
        send(std::string(static_cast<const char *>(data), len));
    }

    // 主动发送：
    /*
    用户调用TcpConnection::send
    如果正好在ioloop内，直接调用TcpConnection::sendInLoop()，
    否则，向ioloop的任务队列中添加TcpConnection::sendInLoop()异步回调。
    执行TcpConnection::sendInLoop()
    如果连接状态为kDisconnected，说明连接断开，直接返回。
    先直接调用::write，能写多少是多少，触发errno == EPIPE || errno == ECONNRESET错误就直接返回。
    如果写完了，异步调用一下writeComplateCallback_回调。否者，说明底层的发送缓存满了，
    剩余的数据追加到outputBuffer_，并使能channel_的写事件，异步通知写outputBuffer_。
    当然，如果outputBuffer_积累的数据太多，达到阈值，就异步调用一下highWaterMarkCallback_。

    */
    void TcpConnection::send(const std::string &message)
    {
        LOG_TRACE << "TcpConnection::send: " << message.size();
        if (state_ == StateE::kConnected)
        {
            if (loop_->isInLoopThread())
            {
                LOG_TRACE << "call sendInLoop";
                sendInLoop(message);
            }
            else
            {
                void (TcpConnection::*fp)(const std::string &) = &TcpConnection::sendInLoop;
                // 按值绑定 message：跨线程时拷贝进 functor，确保 loop 线程执行前数据仍存活。
                // （原先绑定 message.c_str() 会传入悬垂指针，导致发送出 6 字节指针垃圾。）
                loop_->runInLoop(std::bind(fp, this, message));
            }
        }
    }
    void TcpConnection::send(shanchuan::Buffer *buf)
    {
        LOG_TRACE<<"Bffer *buf";
        if (state_ == StateE::kConnected)
        {
            if (loop_->isInLoopThread())
            {
                sendInLoop(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
            }
            else
            {
                void (TcpConnection::*fp)(const std::string &) = &TcpConnection::sendInLoop;
                loop_->runInLoop(std::bind(fp, this, buf->retrieveAllAsString()));
            }
        }
    }
    void TcpConnection::sendInLoop(const std::string &message)
    {
        LOG_TRACE<<"message size : "<<message.size();
        sendInLoop(message.c_str(), message.size());
    }

    void TcpConnection::sendInLoop(const void *data, size_t len)
    {
        LOG_TRACE << "TcpConnection::sendInLoop len=" << len;
        loop_->assertInLoopThread();
        ssize_t nwrote = 0;      // 向TCP缓冲区写了多少数据
        size_t remaining = len;  // 还剩多少要发送的数据
        bool faultError = false; // 向TCP缓冲区发送数据的时候是否出错了
        ////如果链接是断开状态，直接返回
        if (state_ == StateE::kDisconnected)
        {
            LOG_WARN << "disconnected . give up writing";
            return;
        }
        // channel 第一次开始写数据，并且写缓冲区没有数据
        LOG_TRACE << "channel_->isWriting: " << channel_->isWriting();
        LOG_TRACE << "outputBuffer_.readableBytes(): " << outputBuffer_.readableBytes();
        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
        { // 尝试直接向TCP缓冲区发送数据,能写多少是多少
            nwrote = Sockets::write(channel_->fd(), data, len);
            LOG_TRACE << "Sockets::write ret n: " << nwrote;
            if (nwrote >= 0)
            {
                remaining = len - nwrote;
                if (remaining == 0 && writeComplateCallback_)
                {
                    LOG_TRACE << "(remaining == 0 && writeComplateCallback_)";
                    // 放到具有线程安全的任务队列，缓冲区为0，触发低水位回调
                    loop_->queueInLoop(std::bind(writeComplateCallback_, shared_from_this()));
                }
            }
            else
            {
                LOG_TRACE << "nwrote < 0";
                nwrote = 0;
                if (errno != EWOULDBLOCK)
                {
                    LOG_SYSERR << "TcpConnection::sendInLoop";
                    if (errno == EPIPE || errno == ECONNRESET)
                    {
                        faultError = true;
                    }
                }
            }
        }
        assert(remaining <= len);
        // 进入这个代码块里说明，前面向TCP写入数据成功了，并且只写入了一部分数据，
        // 剩下的数据需要保存到缓冲区中，然后启动写事件监控
        // 也可能是因为outputBuffer_里有数据，没有尝试向TCP缓冲区里发送数据

        if (!faultError && remaining > 0)
        {
            LOG_TRACE << "(!faultError && remaining > 0)";
            size_t oldLen = outputBuffer_.readableBytes();
            if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
            {
                // 这里说明缓冲区的数据大小超过了高水位线，并且用户设置高水位线的回调
                loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
            }
            // 把数据保存到缓冲区
            outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);
            // 启动写事件监控
            if (!channel_->isWriting())
            {
                channel_->enableWriting();
            }
        }
        LOG_TRACE << "TcpConnection::sendInLoop end ";
    }

    /*
    细节明细疑问：为什么 muduo 要设计一个 shutdown() 半关闭TCP连接？
    解答：用 shutdown 而不用 close 的效果是，如果对方已经发送了数据，这些数据还“在路上”，
    那么 muduo 不会漏收这些数据。换句话说，muduo 在 TCP 这一层面解决了“当你打算关闭网络连接的时候，
    如何得知对方有没有发了一些数据而你还没有收到？”这一问题。
    当然，这个问题也可以在上面的协议层解决，双方商量好不再互发数据，就可以直接断开连接。
    完整的流程是：    我们发完了数据，于是 shutdownWrite，发送 TCP FIN 分节，对方会读到 0 字节，
    然后对方通常会关闭连接，这样 muduo 会读到 0 字节，然后 muduo 关闭连接。
    */
    void TcpConnection::shutdown()
    {
        LOG_TRACE<<"shotdown"<<stateToString();
        if (StateE::kConnected == state_)
        {
            setState(StateE::kDisconnecting);
            // socket_->shutdownWrite();
            loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
        }
    }


    void TcpConnection::shutdownInLoop()
    {
        LOG_TRACE<<" shutdownInLoop" <<channel_->isWriting();
        loop_->assertInLoopThread();
        if (!channel_->isWriting())
        {
            socket_->shutdownWrite();
        }
    }

    /*
    主动关闭：
    主动调用TcpConnection::forceClose。
    将连接状态设置成kDisconnecting
    异步回调TcpConnection::forceCloseInLoop()
    调用handleClose()
    将连接状态设置成kDisconnected
    取消channel_所有事件
    调用connectionCallback_
    调用closeCallback_（即将TcpServer上的连接信息删除掉）
    异步回调TcpConnection::connectDestroyed
    将channel从Poller中移除。
    */
    void TcpConnection::forceClose()
    {
        LOG_TRACE<<"BEGIN state_: "<<stateToString();
        if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting)
        {
            setState(StateE::kDisconnecting);
            loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
        }
    }
    void TcpConnection::forceCloseInLoop()
    {
        LOG_TRACE<<"BEGIN state_ : "<<stateToString();
        loop_->assertInLoopThread();
        if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting)
        {
            handleClose();
        }
    }
    void TcpConnection::forceCloseWithDelay(double seconds)
    {
        LOG_TRACE<<"seconds: "<<seconds;
        LOG_TRACE<<"state_: "<<stateToString();
        if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting)
        {
            setState(StateE::kDisconnecting);
            loop_->runAfter(
                seconds,
                muduo::makeWeakCallback(shared_from_this(),
                                        &TcpConnection::forceClose));
            // not forceCloseInLoop to avoid race condition
        }
    }

    void TcpConnection::setTcpNoDelay(bool on)
    {
        socket_->setTcpNoDelay(on);
    }
    void TcpConnection::startRead()
    {
        LOG_TRACE<<"start: ";
        loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
    }
    void TcpConnection::startReadInLoop()
    {
        LOG_TRACE<<"start";
        loop_->assertInLoopThread();
        if (!reading_ || !channel_->isReading())
        {
            channel_->enableReading();
            reading_ = true;
        }
    }
    void TcpConnection::stopRead()
    {
        LOG_TRACE<<" stop ";
        loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
    }

    void TcpConnection::stopReadInLoop()
    {
        LOG_TRACE<<"stop ";
        loop_->assertInLoopThread();
        if (reading_ || channel_->isReading())
        {
            channel_->disableReading();
            reading_ = false;
        }
    }

    // called when TcpServer accepts a new connection
    // 连接已建立
    /*在绑定的ioloop中执行TcpConnection::connectEstablished()，
    进行连接的初始化，过程如下：
    将TcpConnection::state_设置成kConnected。
    将channel_的生命周期和TcpConnection绑定，以免TcpConnection被销毁后，channel的回调继续错误的被执行。
    向ioloop的Poller中注册channel_并使能读事件。
    调用TcpConnection::connectionCallback_回调。
    连接建立完毕。连接已建立*/
    void TcpConnection::connectEstablished()
    {
        LOG_TRACE<<"state_: "<<stateToString();
        loop_->assertInLoopThread();
        assert(StateE::kConnecting == state_);
        setState(StateE::kConnected);
        LOG_INFO << "连接已建立";
        channel_->tie(shared_from_this());
        channel_->enableReading(); // 使能读事件
        connectionCallback_(shared_from_this());
        LOG_TRACE<<"END";
    }
    // called when TcpServer has removed me from its map
    void TcpConnection::connectDestroyed() // 终止连接
    {
        LOG_TRACE<<"destroyed: ";
        loop_->assertInLoopThread();
        LOG_TRACE << "state_: " << stateToString();
        if (StateE::kConnected == state_)
        {
            setState(StateE::kDisconnected);
            channel_->disableAll();
            LOG_INFO << "终止连接";
            connectionCallback_(shared_from_this());
        }
        channel_->remove();
        LOG_TRACE<<"END: "<<" state_: "<<stateToString();
    }

    /*接收数据：全权由读回调接收：
    将数据读到TcpConnection::inputBuffer_，返回值n（读到字节数）
    n > 0，调用TcpConnection::messageCallback_处理数据
    n == 0，说明连接关闭，调用TcpConnection::handleClose()回调。
    n < 0，出错，调用TcpConnection::handleError处理。
    */
    void TcpConnection::handleRead(Timestamp receiveTime)
    {
        LOG_TRACE<<"receiveTimer"<<receiveTime.toFormattedString();
        loop_->assertInLoopThread();
        // const int buffsize = 8 * 1024;
        // char buff[buffsize] = {};
        // inputBuffer_.clear();
        // ssize_t n = Sockets::read(socket_->fd(), buff, buffsize);
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(socket_->fd(), &savedErrno);
        if (n > 0)
        { // 调用TcpConnection::messageCallback_处理数据
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
        else if (n == 0)
        { // 说明连接关闭，调用TcpConnection::handleClose()
            LOG_TRACE << "对端关闭连接";
            handleClose();
        }
        else
        { // 出错，调用TcpConnection::handleError处理
            errno = savedErrno;
            LOG_SYSERR << "TcpConnection::headleRead fail ";
            handleError();
        }
        LOG_TRACE << "TcpConnection::handleRead end ";
    }
    /*
    被动关闭：
    TcpConnection::handleRead：因为read返回0，代表连接已经被关闭。会被动调用handleClose。
    Channel::handleEventWithGuard：channel_触发POLLHUP事件，连接被关闭，会被动调用handleClose。
    */

    /*
    异步发送：
    因为发送缓存区满了，所以不得不由Poller异步通知来发送数据
    发送缓存未满，Poller触发可写事件，调用TcpConnection::handleWrite()
    保证channel_->isWriting() == true，否则什么也不做输出日志后返回。
    调用::write()发送outputBuffer_数据。
    如果outputBuffer_数据发送完了，取消cahnnel_的写事件，
    并异步调用一下writeComplateCallback_ && 如果连接状态是kDisconnecting，
    执行shutdownInLoop()。
    关闭本端写。
    */
    void TcpConnection::handleWrite()
    {
        LOG_TRACE<<"handle";
        loop_->assertInLoopThread();
        if (channel_->isWriting())
        {
            LOG_DEBUG << " TcpConnection::handleWrite";
            // 调用::write()发送outputBuffer_数据。
            ssize_t n = Sockets::write(channel_->fd(),
                                       outputBuffer_.peek(),
                                       outputBuffer_.readableBytes());
            if (n > 0)
            {
                outputBuffer_.retrieve(n);
                // 如果outputBuffer_数据发送完了，取消cahnnel_的写事件，
                // 并异步调用一下writeComplateCallback_ &&
                // 如果连接状态是kDisconnecting，执行shutdownInLoop()。关闭本端写。
                if (outputBuffer_.readableBytes() == 0)
                {
                    channel_->disableWriting();
                    if (writeComplateCallback_)
                    {
                        loop_->queueInLoop(std::bind(writeComplateCallback_, shared_from_this()));
                    }
                    if (state_ == StateE::kDisconnecting)
                    {
                        shutdownInLoop();
                    }
                }
            }
            else
            {
                LOG_SYSERR << "TcpConnection::handleWrite";
            }
        }
        else
        {
            LOG_TRACE << "Connection fd = " << channel_->fd()
                      << "is down , no more writing ";
        }
    }
    void TcpConnection::handleClose()
    {
        LOG_TRACE<<"Close";
        loop_->assertInLoopThread();
        LOG_TRACE << "fd " << socket_->fd() << "state = " << stateToString();
        assert(state_ == StateE::kConnected || state_ == StateE::kDisconnecting);

        setState(StateE::kDisconnected);
        LOG_TRACE << "fd " << socket_->fd() << " index_ " << channel_->index();
        channel_->disableAll();
        TcpConnectionPtr guardThis(shared_from_this());
        LOG_TRACE << "fd " << socket_->fd() << " index: " << channel_->indextoString();
        LOG_TRACE << "fd " << socket_->fd() << " event : " << channel_->events();
        connectionCallback_(guardThis);
        // 必须是最后一行
        closeCallback_(guardThis);
    }
    void TcpConnection::handleError()
    {
        LOG_TRACE<<"Error";
        int err = Sockets::getSocketError(socket_->fd()); //
        LOG_ERROR << "TcpConnection::handleError [" << name_
                  << "] SO_ERROR= " << err << " " << strerror(err);
    }

    const std::string TcpConnection::stateToString() const
    {
        switch (state_)
        {
        case StateE::kDisconnected:
            return "kDisconnected";
        case StateE::kConnecting:
            return "kConnecting";
        case StateE::kConnected:
            return "kConnected";
        case StateE::kDisconnecting:
            return "kDisconnecting";
        default:
            return "unknown state";
        }
    }
}
