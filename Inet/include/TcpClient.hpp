#include <mutex>
#include <memory>
#include <string>
using namespace std;

#include "TcpConnection.hpp"

#ifndef TCPCLIENT_HPP
#define TCPCLIENT_HPP

namespace shanchuan
{
    class Connector;
    using ConnectorPtr = std::shared_ptr<Connector>;

    class TcpClient
    {
    private:
        EventLoop *loop_;
        ConnectorPtr connector_;// 1
        const string name_;
        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;// read server data;
        WriteComplateCallback writeComplateCallback_;
        bool retry_;
        bool connect_;
        int nextConnId_;
        mutable std::mutex mutex_;
        TcpConnectionPtr connection_;// r w;

    private:
        void newConnection(int sockfd);
        void removeConnection(const TcpConnectionPtr &conn);

    public:
        TcpClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &name);
        ~TcpClient();
        void connect();
        void disconnect();
        void stop();

        TcpConnectionPtr connection() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return connection_;
        }
        EventLoop *getLoop() const { return loop_; }
        bool retry() const { return retry_; }
        void enableRetry() { retry_ = true; }
        void setConnectionCallback(const ConnectionCallback &cb)
        {
            connectionCallback_ = cb;
        }
        void setMessageCallback(const MessageCallback &cb)
        {
            messageCallback_ = cb;
        }
        void setWriteCompleteCallback(const WriteComplateCallback &cb)
        {
            writeComplateCallback_ = cb;
        }
    };

} // namespace shanchuan

#endif