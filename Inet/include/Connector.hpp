

#include <memory>
#include <functional>
#include <vector>
using namespace std;

#include "InetAddress.hpp"
#ifndef CONNECTOR_HPP
#define CONNECTOR_HPP

namespace shanchuan
{
    class Channel;
    class EventLoop;
    class Connector : public std::enable_shared_from_this<Connector>
    {
    public:
        using NewConnectionCallback = std::function<void(int sockfd)>;

    private:
        enum class States
        {
            kDisconnected,
            kConnecting,
            kConnected,
        };
        EventLoop *loop_;
        shanchuan::InetAddress serverAddr_;
        bool connect_;
        States state_;
        std::unique_ptr<Channel> channel_;
        NewConnectionCallback newConnectionCallback_;
        int retryDelayMs_; //
        static const int kMaxRetryDelayMs;  //= 30 * 1000; // 最大重试延迟
        static const int kInitRetryDelayMs; // = 500;      // 初始化重试延迟
    private:
        void setState(const States &s) { state_ = s; }
        void startInLoop();
        void stopInLoop();
        void connect();
        void connecting(int sockfd);
        void handleWrite();
        void handleError();
        void retry(int sockfd);
        int removeAndResetChannel();
        void resetChannel();

    public:
        Connector(EventLoop *loop, const InetAddress &serverAddr);
        ~Connector();
        void setNewConnectionCallback(const NewConnectionCallback &cb)
        {
            newConnectionCallback_ = cb;
        }
        void start();
        void restart();
        void stop();
        const InetAddress &serverAddress() const { return serverAddr_; }
        const std::string  StatesToString() const
        {
            std::string str = "not connected";
            switch (state_)
            {
            case States::kDisconnected:
                str = "已断开连接";
                break;
            case States::kConnecting:
                str = "正在建立连接";
                break;
            case States::kConnected:
                str = "已连接";
                break;
            }
            return str;
        }
    };

} // namespace shanchuan

#endif