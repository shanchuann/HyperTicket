#ifndef TIMERQUEUE_HPP
#define TIMERQUEUE_HPP

#include <set>
#include <vector>
#include <mutex>
#include "Timestamp.hpp"
#include "Callbacks.hpp"
#include "Channel.hpp"

namespace shanchuan
{
    class EventLoop;
    class Timer;
    class TimerId;
    class TimerQueue
    {
    public:
        using Entry = std::pair<shanchuan::Timestamp, Timer *>; // 时间戳和定时器对
        using TimerList = std::set<Entry>;
        using ActiveTimer = std::pair<Timer *, int64_t>; // (定时器，序列号)为key
        using ActiveTimerSet = std::set<ActiveTimer>;
    private:
        EventLoop *loop_;        // 定时器和哪个EventLoop关联
        const int timerfd_;      // timerfd_
        Channel timerfdChannel_; // 基于timerfd_的Channel
        TimerList timers_;
        ActiveTimerSet activeTimers_;            // 活跃的定时器
        std::atomic<bool> callingExpiredTimers_; // 调用到期计时器
        ActiveTimerSet cancelingTimers_;         // 取消的定时器
    private:
        void addTimerInLoop(Timer *timer);
        void cancelInLoop(TimerId timerId);
        void handleRead();
        std::vector<Entry> getExpired(Timestamp now);
        void reset(const std::vector<Entry> &expired, Timestamp now);
        bool insert(Timer *timer);
    public:
        explicit TimerQueue(EventLoop *loop);
        ~TimerQueue();
        TimerId addTimer(TimerCallback cb, Timestamp when, double interval);
        void cancel(TimerId timerId);
    };
} // namespace shanchuan
#endif // TIMERQUEUE_HPP