
#include "Timer.hpp"

#ifndef TIMERID_HPP
#define TIMERID_HPP
// TimerId类用于保存超时任务Timer和它独一无二的id
namespace shanchuan
{
    class TimerId
    {
    private:
        Timer *timer_;
        int64_t sequence_;
        friend class TimerQueue;

    public:
        TimerId() : timer_(nullptr), sequence_(0) {}
        TimerId(Timer *timer, int64_t seq)
            : timer_(timer), sequence_(seq) {}
        ~TimerId() {}
    };
} // namespace shanchuan

#endif