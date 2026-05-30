

#include "Callbacks.hpp"
#include "Timestamp.hpp"

#include <atomic>
using namespace std;

#ifndef TIMER_HPP
#define TIMER_HPP

namespace shanchuan
{
    class Timer
    {
    private:
        const TimerCallback callback_;             // 超时回调函数,回调函数是用户提供的
        shanchuan::Timestamp expiration_;              // 时间戳,定时器的到期时间
        const double interval_;                    // 定时器触发的时间间隔
        const bool repeat_;                        // 定时器是否是重复触发的，即是不是周期性的
        const int64_t sequence_;                   // 定时器序号
        static std::atomic<int64_t> s_sumCreated_; 
        // 定时器计数，当前已创建的定时器数量，原子int64_t类型，初始值为0,
        // 定时器都有个序号
    public:
        Timer(TimerCallback cb, Timestamp when, double interval)
            : callback_(std::move(cb)),
              expiration_(when),
              interval_(interval),
              repeat_(interval > 0.0),
              sequence_(s_sumCreated_.fetch_and(1))
        {
        }
        ~Timer() {}
        
        void run() const
        {
            callback_();
        }
        //获取到期时间
        Timestamp expiration() const { return expiration_;}
        //是否是重复的定时器
        bool repeat() const { return repeat_;}
        //定时器的序号
        int64_t sequence() const { return sequence_;}
        //定时器到期后，若这个定时器是周期性的定时器，那么需要重设
        void restart(Timestamp now);
        static int64_t numCreated() { return s_sumCreated_.load();}
    };
} // namespace shanchuan

#endif