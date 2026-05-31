#ifndef TCPCONNECTION_HPP
#define TCPCONNECTION_HPP

#include "Timestamp.hpp"
#include "InetAddress.hpp"
#include "Socket.hpp"
#include "Callbacks.hpp"
#include "Buffer.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include <netinet/tcp.h>
#include <string>
#include <memory>
#include <atomic>
#include <any>
struct tcp_info;

namespace shanchuan
{
    class Socket;
    void defaultConnectionCallback(const TcpConnectionPtr &conn);
    void defaultMessageCallback(const TcpConnectionPtr &, Buffer *buf, Timestamp);
    class TcpConnection : public std::enable_shared_from_this<TcpConnection>
    {
    private:
        enum class StateE {                 // 枚举类型，包含TCP连接的状态
            kDisconnected,                  // 已断开连接 
            kConnecting,                    // 在建立连接中
            kConnected,                     // 已建立连接
            kDisconnecting                  // 在断开连接中
        };
        EventLoop *loop_;                   // 指针指向所属的EventLoop对象
        const std::string name_;            // 连接名
        std::atomic<StateE> state_;         // 连接的状态
        bool reading_;                      // 连接是否正在监听读事件
        std::unique_ptr<Socket> socket_;    // 套接字成员socket connfd接字, 用于对连接进行底层操作
        const InetAddress localAddr_;       // 本地IP地址
        const InetAddress peerAddr_;        // 对端IP地址

        //连接到来的回调函数connectionCallback_，由TcpServer::newConnection()函数设置
        ConnectionCallback connectionCallback_;
        //消息到来的回调函数messageCallback_，由TcpServer::newConnection()函数设置
        MessageCallback messageCallback_;
        // 数据发送完毕时的回调函数，即所有的用户数据都已拷贝到内核缓冲区时
        // 回调该函数，outputBuffer_被清空也会回调该函数，可以理解为低水位标回调函数
        WriteComplateCallback writeComplateCallback_;
        // 高水位标回调函数
        HighWaterMarkCallback highWaterMarkCallback_;
        //连接断开时的回调函数closeCallback_
        CloseCallback closeCallback_;
        size_t highWaterMark_; //高水位标
        //因为发送数据，应用写得快，内核发送数据慢，需要把待发送的数据写入缓冲区，且设置了水位回调，防止发送太快
        shanchuan::Buffer inputBuffer_;     // 应用层接收缓冲区
        shanchuan::Buffer outputBuffer_;    // 应用层发送缓冲区
        std::any context_;                  //绑定一个未知类型的上下文对象
        std::unique_ptr<Channel> channel_;  //通道成员channel_

        //通道可读事件到来的时候，回调的函数
        void handleRead(Timestamp receiveTime); 
        void handleWrite(); // 处理写事件
        void handleClose(); // 通道关闭事件到来的时候，回调的函数
        void handleError(); // 通道出现错误事件的时候，回调的函数
        //被send()调用，用来发送数据
        void sendInLoop(const std::string &message);
        void sendInLoop(const void *message,size_t len);
        //被shutdown()调用，关闭连接写的这一半
        void shutdownInLoop();
        void forceCloseInLoop();
        void setState(const StateE &s) { state_ = s;}
        void startReadInLoop();
        void stopReadInLoop();
    public:
        void setContext(const std::any &context) { context_ = context; }
        const std::any &getContext() const { return context_; }
        std::any *getMutableContext() { return &context_; }
        void setConnectionCallback(const ConnectionCallback &cb) {          //设置连接到来或者连接关闭回调函数
            connectionCallback_ = cb;
        }
        void setMessageCallback(const MessageCallback &cb) {                //设置消息到来回调函数
            messageCallback_ = cb;
        }
        void setWriteComplateCallback(const WriteComplateCallback &cb) {    //设置数据发送完毕时的回调函数
            writeComplateCallback_ = cb;
        }
        //设置高水位标回调函数
        void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
            highWaterMarkCallback_ = cb;
            highWaterMark_ = highWaterMark;
        }
        //设置连接断开回调函数
        void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
        // 返回应用层接收缓冲区inputBuffer_
        shanchuan::Buffer &inputBuffer()  { return inputBuffer_;  }
        shanchuan::Buffer &outputBuffer() { return outputBuffer_; }
        // 当TcpServer接受新连接时调用,只应调用一次,当新连接建立时调用该函数
        void connectEstablished();
        // 当TcpServer将我从其映射中删除时调用,只应调用一次,当连接断开时调用的函数
        void connectDestroyed();

        TcpConnection(EventLoop *loop, const string &nameArg, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr);
        ~TcpConnection();
        const std::string &name() const { return name_; }
        const InetAddress &localAddress() const { return localAddr_; }
        const InetAddress &peerAddress()  const { return peerAddr_;  }
        //判断当前tcp连接是否已经连接
        bool connected() const { return StateE::kConnected == state_; }
        //将本sockfd的tcp_info内容放置到指针中
        bool getTcpInfo(struct tcp_info *) const;
        std::string getTcpInfoString() const;
        void send(const void *message,int len);     //发送长度为len的message
        void send(const std::string &message);      //发送一个string类型的message
        void send(Buffer * message);                //发送一个Buffer类型的message
        void shutdown();                            //关闭连接写的这一半       
        void forceClose();                          //强制关闭
        void forceCloseWithDelay(double seconds);   //强制延迟关闭
        void setTcpNoDelay(bool on);                //禁用Nagle算法，可以避免连续发包出现延迟
        const std::string stateToString() const;
        void startRead();
        void stopRead();
        bool IsReading() const { return reading_;}
        EventLoop *getLoop() const { return loop_; }
    };
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
} // namespace shanchuan
#endif // TCPCONNECTION_HPP