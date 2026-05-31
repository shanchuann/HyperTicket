#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <string>
#include <memory>
#include <map>
#include <atomic>
#include <mutex>
#include "Callbacks.hpp"
#include "TcpConnection.hpp"

namespace shanchuan
{
    class Acceptor;
    class EventLoop;
    class EventLoopThreadPool;

    class TcpServer
    {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;
        using ConnectionMap = std::map<std::string, TcpConnectionPtr>;
        enum Option {
            kNoReusePort, // 端口号重复使用
            kReusePort,
        };
    private:
        EventLoop *loop_;                                   // the acceptor loop
        const std::string hostport_;                        // 服务的ip:端口
        const std::string name_;                            // 服务名
        std::unique_ptr<Acceptor> acceptor_;                // 接受连接对象
        std::shared_ptr<EventLoopThreadPool> threadPool_;

        ConnectionCallback connectionCallback_;             // 连接回调
        MessageCallback messageCallback_;                   // 消息回调
        WriteComplateCallback writeComplateCallback_;       // 写完整回调；
        ThreadInitCallback threadInitCallback_;             // 线程初始化回调
        std::atomic<int> started_;                          // 开始
        int nextConnId_;                                    // 下一个连接ID,每次增加一个就加1
        ConnectionMap connections_;                         // 全相关连接对象列表
        mutable std::mutex connectionsMutex_;               // 保护 connections_ 的互斥锁

        // 线程不安全, 只能在loop线程中调用
        void newConnection(int sockfd, const InetAddress &peerAddr);
        // 线程安全
        void removeConnection(const TcpConnectionPtr &conn);
        // 现在是线程安全的（使用互斥锁）
        void removeConnectionInLoop(const TcpConnectionPtr &conn, EventLoop *ioLoop);
    public:
        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
        void setWriteCompleteCallback(const WriteComplateCallback &cb) { writeComplateCallback_ = cb; }
        void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
        EventLoop * getLoop() const { return loop_;}
        std::shared_ptr<EventLoopThreadPool> threadPool() const { return threadPool_;}

        TcpServer(EventLoop *loop, const InetAddress &listenAddr, const string &nameArg, Option option = kNoReusePort);
        ~TcpServer();
        const std::string &hostport() const { return hostport_; }
        const std::string &name() const { return name_; }
        void start();
        void setThreadNum(int numThreads);
    };
} // namespace shanchuan
#endif // TCPSERVER_HPP