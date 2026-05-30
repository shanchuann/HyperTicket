#include "Logger.hpp"
#include "Timer.hpp"

#include <atomic>

namespace shanchuan
{
    std::atomic<int64_t> Timer::s_sumCreated_{0};
    void Timer::restart(shanchuan::Timestamp now)
    {
        LOG_TRACE<<" now  "<<now.toFormattedString();
        if(repeat_) expiration_ = shanchuan::addTime(now,interval_);
        else expiration_ = shanchuan::Timestamp::invalid();
    }
} // namespace shanchuan