//C++ STL
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <any>
using namespace std;

#include "Timestamp.hpp"
#include "TimerId.hpp"
#include "TimerQueue.hpp"
#include "Callbacks.hpp"

#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

namespace shanchuan
{
    class Channel;
    class Poller;
    class EventLoop
    {
    public:
        using Functor = std::function<void(void)>;
    private:
        using ChannelList = std::vector<Channel *>;
        bool looping_;               // true表示loop循环执行中
        std::atomic<bool> quit_;     // loop循环退出条件
        bool eventHandling_;         // true表示loop循环正在处理事件回调
        int64_t iteration_;         // loop迭代次数//
        // const pid_t threadId_;          // 线程id, 对象构造时初始化
        const std::thread::id threadId_;
        Timestamp pollReturnTime_;             // poll()返回时间点
        std::unique_ptr<Poller> poller_;       // 轮询器, 用于监听事件
        std::unique_ptr<TimerQueue> timerQueue_;// 
        ChannelList activeChannels_;           // 激活事件的通道列表//
        Channel *currentActiveChannel_;        // 当前激活的通道, 即正在调用事件回调的通道
    private:
        int wakeupFd_;                              // 唤醒loop线程的eventfd
        std::unique_ptr<Channel> wakeupChannel_;    // 用于唤醒loop线程的channel
        std::any context_;
        bool callingPendingFunctors_;                // true表示loop循环正在调用待定函数
        mutable std::mutex mutex_;
        // 待调用函数列表, 
        std::vector<Functor> pendingFunctors_; // guarded_by(mutex_);
        // 处理pending函数
        void doPendingFunctors();
        void handleRead(); // waked up
    public:
        // 在loop线程中, 立即运行回调cb.如果没在loop线程, 就会唤醒loop, (排队)运行回调cb.
        // 如果用户在同一个loop线程, cb会在该函数内运行; 否则， 会在loop线程中排队运行.
        // 因此, 在其他线程中调用该函数是安全的.
        void runInLoop(const Functor &cb);
        // 排队回调cb进loop线程 ,回调cb在loop中完成polling后运行.从其他线程调用是安全的.
        void queueInLoop(const Functor &cb);
        // 排队的回调cb个数
        size_t queueSize() const;
        // 唤醒loop线程, 没有事件就绪时, loop线程可能阻塞在poll()/epoll_wait()
        void wakeup();

        // 判断是否有待调用的回调函数(pending functor)
        bool callingPendingFunctors() const
        {
            return callingPendingFunctors_;
        }

        /// Runs callback at 'time'.
        /// Safe to call from other threads.
        TimerId runAt(Timestamp time, TimerCallback cb);
        /// Runs callback after @c delay seconds.
        /// Safe to call from other threads.
        TimerId runAfter(double delay, TimerCallback cb);
        // Runs callback every @c interval seconds.
        /// Safe to call from other threads.
        TimerId runEvery(double interval, TimerCallback cb);
        ///
        /// Cancels the timer.
        /// Safe to call from other threads.
        ///
        void cancel(TimerId timerId);

    public:
        EventLoop();
        ~EventLoop();
        // loop循环, 运行一个死循环.必须在当前对象的创建线程中运行.
        void loop();
        // 退出loop循环. 如果通过原始指针(raw pointer)调用, 不是100% 线程安全;
        void quit();
        // Poller::poll()返回的时间, 通常意味着有数据达到.
        // 对于PollPoller, 是调用完poll(); 对于EPollPoller, 是调用完epoll_wait()
        Timestamp pollReturnTime() const { return pollReturnTime_; }
        // 获取loop循环次数
        int64_t iteration() const { return iteration_; }
        // 更新Poller监听的channel, 只能在channel所属loop线程中调用
        void updateChannel(Channel *channel);
        // 移除Poller监听的channel, 只能在channel所属loop线程中调用
        void removeChannel(Channel *channel);
        // 判断Poller是否正在监听channel, 只能在channel所属loop线程中调用
        bool hasChannel(Channel *channel);
        // 断言当前线程是创建当前对象的线程, 如果不是就终止程序(LOG_FATAL)
        void assertInLoopThread()
        {
            if (!isInLoopThread())
            {
                abortNotInLoopThread();
            }
        }
        // 判断前线程是否创建当前对象的线程.
        // threadId_是创建当前EventLoop对象时, 记录的线程tid
        // bool isInLoopThread() const 
        //{ return threadId_ == CurrentThread::tid(); }
        bool isInLoopThread() const 
        { 
            return threadId_ == std::this_thread::get_id();
         }

        // 判断loop线程是否正在处理事件, 执行事件回调.
        // loop线程正在遍历,执行激活channels时, eventHandling_会置位; 其余时候, 会清除.
        bool eventHandling() const { return eventHandling_; }
        void setContext(const std::any & context)
        {
            context_ = context;
        }
        const std::any & getContext() const
        {
            return context_;
        }
        std::any * getMutableContext()
        {
            return &context_;
        }
        /* 获取当前线程的EventLoop对象指针 */
        static EventLoop *getEventLoopOfCurrentThread();

    private:
        // 终止程序(LOG_FATAL), 当前线程不是创建当前EventLoop对象的线程时,
        // 由assertInLoopThread()调用
        void abortNotInLoopThread();
        // 唤醒所属loop线程, 也是wakeupFd_的事件回调
        // void handleRead();  // waked up
        // 打印激活通道的事件信息, 用于debug
        void printActiveChannels() const; // DEBUG
    };
}

#endif