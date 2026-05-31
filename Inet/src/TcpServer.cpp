// C API
#include <assert.h>
// C++ STL
#include <functional>
using namespace std;
// own
#include "Callbacks.hpp"
#include "Logger.hpp"
#include "TcpConnection.hpp"
#include "Acceptor.hpp"
#include "SocketsOps.hpp"
#include "TcpServer.hpp"
#include "EventLoop.hpp"
#include "EventLoopThreadPool.hpp"

namespace shanchuan
{
    void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
    {
        loop_->assertInLoopThread();
        // EventLoop *ioLoop = loop_; //?
        EventLoop *ioLoop = threadPool_->getNextLoop();
        LOG_TRACE << "TcpServer::newConnection";
        char buff[128] = {};
        snprintf(buff, sizeof(buff) - 1, ":%s#%d", hostport_.c_str(), nextConnId_);
        ++nextConnId_;
        std::string connName(name_ + buff);
        LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection [" << connName << "] from " << peerAddr.toIpPort();
        InetAddress localAddr(Sockets::getLocalAddr(sockfd));
        LOG_INFO << "localAddr: " << localAddr.toIpPort();
        TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                                connName,
                                                sockfd,
                                                localAddr,
                                                peerAddr));
        // TcpConnectionPtr conn = std::make_shared<TcpConnection>()
        connections_[connName] = conn;
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteComplateCallback(writeComplateCallback_);
        conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

        // 设置连接建立
        ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
        //conn->connectEstablished();
        LOG_TRACE<<"end";
    }
    TcpServer::TcpServer(EventLoop *loop,
                         const InetAddress &listenAddr,
                         const string &nameArg,
                         Option option)
        : loop_((assert(loop != NULL), loop)),
          hostport_(listenAddr.toIpPort()),
          name_(nameArg),
          acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
          threadPool_(new EventLoopThreadPool(loop)),
          connectionCallback_(shanchuan::defaultConnectionCallback),
          messageCallback_(shanchuan::defaultMessageCallback),
          started_(0),
          nextConnId_(1)
    {
        LOG_INFO << "TcpServer::TcpServer this=" << this << ", loop_=" << loop_
                 << ", thread=" << std::this_thread::get_id();
        acceptor_->setNewConnectionCallback(
            std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
    }

    TcpServer::~TcpServer()
    {
        loop_->assertInLoopThread();
        LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";
        // for (ConnectionMap::iterator it = connections_.begin(); it != connections_.end(); ++it)
        for (auto it = connections_.begin(); it != connections_.end(); ++it)
        {
            TcpConnectionPtr conn = it->second;
            it->second.reset();
            // 使用 queueInLoop 确保异步执行，避免在析构时的竞态
            conn->getLoop()->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
            conn.reset(); //
        }
    }

    void TcpServer::setThreadNum(int numThreads)
    {
        assert(0 <= numThreads);
        threadPool_->setThreadNum(numThreads);
    }
    void TcpServer::start()
    {
        LOG_TRACE << "TcpServer::start";
        // fetch_add :原子地将实参加到存储于原子对象的值上，并返回先前保有的值
        if (started_.fetch_add(1) == 0)
        {
            threadPool_->start(threadInitCallback_);
            assert(!acceptor_->listenning());
            // acceptor_->listen();
            loop_->runInLoop(
                std::bind(&Acceptor::listen, get_pointer(acceptor_)));
        }
    }
    void TcpServer::removeConnection(const TcpConnectionPtr &conn)
    {
        LOG_INFO << "removeConnection: this=" << this << ", loop_=" << loop_
                 << ", thread=" << std::this_thread::get_id();

        // 关键修复：在这里获取 ioLoop，避免在回调中访问可能已析构的 conn
        EventLoop *ioLoop = conn->getLoop();

        // 使用 queueInLoop 而不是 runInLoop，确保总是异步执行
        loop_->queueInLoop([this, conn, ioLoop]() {
            this->removeConnectionInLoop(conn, ioLoop);
        });
    }

    void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn, EventLoop *ioLoop)
    {
        LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
                 << "] - connection " << conn->name();

        // 使用互斥锁保护 connections_ 映射
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            size_t n = connections_.erase(conn->name());
            (void)n;
            assert(n == 1);
        }

        // 关键修复：TcpConnection 的析构必须在其所属的 IO 线程中进行
        // 将 conn 的最后一个引用投递回 IO 线程
        ioLoop->queueInLoop([conn]() {
            // conn 在这个 lambda 中被捕获，当 lambda 析构时，conn 也会析构
            // 此时在 IO 线程中，所以是安全的
            LOG_TRACE << "TcpConnection " << conn->name() << " will destruct in IO thread";
        });

        LOG_TRACE << "Connection removed from map";
    }
}
