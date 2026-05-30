// C API
#include <assert.h>
// C++ STL
#include <functional>
#include <memory>
using namespace std;

#include"Logger.hpp"
#include "Timestamp.hpp"

#ifndef CHANNEL_HPP
#define CHANNEL_HPP
namespace shanchuan
{
    //static const int kNew = -1;    // Channel 要添加到map和epoll红黑树中
    //static const int kAdded = 1;   // Channel 在map 和 epoll 的红黑树中
    //static const int kDeleted = 2; // Channel 在map中，但是不在epoll 的红黑树中
    class EventLoop;

    class Channel
    {
    public:
        // 事件回调函数定义
        using EventCallback = std::function<void()>;
        // 读事件回调函数定义
        using ReadEventCallback = std::function<void(Timestamp)>;

    private:
        // 事件状态设置会使用
        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

    private:
        EventLoop *loop_;    // 当前Channel 关联的 EventLoop
        const int fd_;       // Poller监听的文件描述符
        int events_;         // 注册fd感兴趣的事件
        int revents_;        // poller返回的具体发生的事件
        int index_;          // 在Poller上注册的情况： KNew kAdded kDeleted
       
        std::weak_ptr<void> tie_; // 多线程中监听对象的生存情况
        bool tied_;               // 捆绑
        bool eventHandling_; // 是否处于事件循环中（正处理事件）
        bool addedToLoop_;   // 是否已被添加到事件循环
        // 处理事件的回调函数
        ReadEventCallback readCallback_;
        EventCallback writeCallback_;
        EventCallback closeCallback_;
        EventCallback errorCallback_;
        // 注册事件后更新到EventLoop 关联的EPoller对象
        void update();
        // 使用防护处理事件               //接收时间
        void handleEventWithGuard(Timestamp receiveTime);

    public:
        // 处理事件
        void handleEvent(Timestamp receiveTime);
        Channel(EventLoop *loop, int fd);
        ~Channel();
        // 从epoll对象中移除
        void remove(); //
        // for Poller
        int index() { return index_; }
        void set_index(int idx) { index_ = idx; }
        std::string indextoString() const
        {
            switch (index_)
            {
            case 1:
                return std::string("kAdded");
                break;
            case -1:
                return std::string("kNew");
                break;
            case 2:
                return std::string("kDeleted");
                break;
            }
            return std::string("error index: ");
        }
        // 设置可读、可写、关闭、出错的事件回调函数
        void setReadCallback(const ReadEventCallback &cb)
        {
            readCallback_ = cb;
        }
        void setWriteCallback(const EventCallback &cb)
        {
            writeCallback_ = cb;
        }
        void setCloseCallback(const EventCallback &cb)
        {
            closeCallback_ = cb;
        }
        void setErrorCallback(const EventCallback &cb)
        {
            errorCallback_ = cb;
        }

        void tie(const std::shared_ptr<void> &);
        int fd() const { return fd_; }
        // 返回注册的事件
        int events() const { return events_; }
        // 设置发生的事
        void set_revents(int revt) { revents_ = revt; }
        // int revents() const { return revents_; }
        // 判断是否注册了事件
        bool isNoneEvent() const { return events_ == kNoneEvent; }

        // 注册可读事件
        void enableReading()
        {
            events_ |= kReadEvent;
            update();
        }
        // 注销可读事件
        void disableReading()
        {
            events_ &= ~kReadEvent;
            update();
        }
        // 注册可写事件
        void enableWriting()
        {
            LOG_TRACE<<"注册可写事件";
            events_ |= kWriteEvent;
            update();
        }
        // 注销可写事件
        void disableWriting()
        {
            events_ &= ~kWriteEvent;
            update();
        }
        // 注销所有事件
        void disableAll()
        {
            events_ = kNoneEvent;
            update();
        }
        bool isWriting() const { return events_ & kWriteEvent; }
        bool isReading() const { return events_ & kReadEvent; }
        EventLoop *ownerLoop() { return loop_; }
        
        string reventsToString() const;

        bool getEventHandling() const { return eventHandling_;}
    };
}
#endif