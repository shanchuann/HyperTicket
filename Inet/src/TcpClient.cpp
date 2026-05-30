#include "Logger.hpp"
#include "Connector.hpp"
#include "EventLoop.hpp"
#include "SocketsOps.hpp"
#include "TcpClient.hpp"
#include <functional>

using namespace std;

#include <stdio.h>

namespace shanchuan
{
    void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn)
    {
        loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
    void removeConnector(const ConnectorPtr &connector)
    {
    }

    void TcpClient::newConnection(int sockfd)
    {
        LOG_TRACE << "sockfd: " << sockfd;
        const int len = 128;
        loop_->assertInLoopThread();
        shanchuan::InetAddress peerAddr(Sockets::getPeerAddr(sockfd));
        char buff[len] = {0};
        snprintf(buff, sizeof(buff), ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
        ++nextConnId_;
        string connName = name_ + buff;

        shanchuan::InetAddress localAddr(Sockets::getLocalAddr(sockfd));
        TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));

        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connection_ = conn;
        }
        conn->connectEstablished();
        // 连接已建立
        LOG_TRACE << "connection to [" << conn->peerAddress().toIpPort() << "] established";
    }
    void TcpClient::removeConnection(const TcpConnectionPtr &conn)
    {
        LOG_TRACE << "BEGIN";
        loop_->assertInLoopThread();
        assert(loop_ == conn->getLoop());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            assert(connection_ == conn);
            connection_.reset();
        }
        loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
        LOG_TRACE<<"retry_: "<<retry_;
        LOG_TRACE<<"connect_: "<<connect_;
        
        if (retry_ && connect_)
        {
            LOG_INFO << "TcpClient::conect[" << name_ << "] - Reconnection to"
                     << connector_->serverAddress().toIpPort();
            connector_->restart();
        }
    }

    TcpClient::TcpClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &name)
        : loop_(loop),
          connector_(new Connector(loop, serverAddr)),
          name_(name),
          connectionCallback_(defaultConnectionCallback),
          messageCallback_(defaultMessageCallback),
          retry_(false),
          connect_(true),
          nextConnId_(1)
    {
        connector_->setNewConnectionCallback(
            std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
        LOG_INFO << "TcpClient::TcpClient [" << name_ << "] - connector "; // << (size_t)get_pointer(connector_);
    }
    TcpClient::~TcpClient()
    {
        LOG_INFO << "TcpClient::~TcpClient[" << name_ << "] -connector "; //<< (size_t)get_pointer(connector_);
        TcpConnectionPtr conn;
        bool unique = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            unique = connection_.unique();
            conn = connection_;
        }
        if (conn)
        {
            assert(loop_ == conn->getLoop());
            CloseCallback cb = std::bind(shanchuan::removeConnection, loop_, std::placeholders::_1);
            loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
            if (unique)
            {
                conn->forceClose();
            }
        }
        else
        {
            connector_->stop();
            loop_->runAfter(1, std::bind(shanchuan::removeConnector, connector_));
        }
    }
    void TcpClient::connect()
    {
        LOG_INFO << "TcpClient::connect [" << name_ << "] - connection to "
                 << connector_->serverAddress().toIpPort();
        connect_ = true;
        connector_->start();
    }
    void TcpClient::disconnect()
    {
        LOG_TRACE << "BEGIN";
        connect_ = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (connection_)
            {
                connection_->shutdown();
            }
        }
        LOG_TRACE << "END";
    }
    void TcpClient::stop()
    {
        LOG_TRACE << "BEGIN";
        connect_ = false;
        connector_->stop();
    }

} // namespace shanchuan
