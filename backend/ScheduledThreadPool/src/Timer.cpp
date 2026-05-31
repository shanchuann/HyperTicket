#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "../include/Timer.hpp"
#include "Logger.hpp"

namespace shanchuan::scheduled
{
    struct timespec toTimeSpec(const shanchuan::Timestamp &timestamp)
    {
        struct timespec ts;
        int64_t micro = static_cast<int64_t>(timestamp.getMicro()) - static_cast<int64_t>(shanchuan::Timestamp::Now().getMicro());
        if (micro < 1000) // ensure at least 1ms
        {
            micro = 1000;
        }
        if (micro < 0)
            micro = 1000;
        ts.tv_sec = static_cast<time_t>(micro / shanchuan::Timestamp::kMinPerSec);
        ts.tv_nsec = static_cast<long>((micro % shanchuan::Timestamp::kMinPerSec) * 1000); // microsecond -> nanosecond
        return ts;
    }
    bool Timer::setTimer()
    {
        bool ret = true;
        struct itimerspec newvalue = {};
        if (_is_repeating)
        {
            // _interval is in milliseconds
            newvalue.it_interval.tv_sec = static_cast<time_t>(_interval / 1000);
            newvalue.it_interval.tv_nsec = static_cast<long>((_interval % 1000) * 1000000); // ms -> ns
        }
        else
        {
            newvalue.it_interval.tv_sec = 0;
            newvalue.it_interval.tv_nsec = 0;
        }
        newvalue.it_value = toTimeSpec(_expiration);
        if (::timerfd_settime(_timerfd, 0, &newvalue, nullptr) != 0)
        {
            LOG_ERROR << "Failed to set timer: " << strerror(errno);
            ret = false;
        }
        return ret;
    }
    Timer::Timer() : _timerfd(-1), _callback(nullptr), _expiration(), _interval(0), _is_repeating(false)
    {
        LOG_DEBUG << "Timer constructed";
    }
    Timer::~Timer()
    {
        closeTimer();
    }
    Timer::Timer(Timer &&other) noexcept : _timerfd(other._timerfd), _callback(std::move(other._callback)), _interval(other._interval)
    {
        other._timerfd = -1;
        other._interval = 0;
        other._callback = nullptr;
    }
    Timer &Timer::operator=(Timer &&other) noexcept
    {
        if (this != &other)
        {
            closeTimer();
            _timerfd = other._timerfd;
            _callback = std::move(other._callback);
            _interval = other._interval;
            other._timerfd = -1;
            other._interval = 0;
            other._callback = nullptr;
        }
        return *this;
    }
    bool Timer::init(timerCallBack cb, const shanchuan::Timestamp &when, size_t interval)
    {
        bool ret = true;
        _timerfd = ::timerfd_create(CLOCK_MONOTONIC, 0);
        if (_timerfd < 0)
        {
            LOG_ERROR << "Failed to create timerfd: " << strerror(errno);
            ret = false;
        }
        else
        {
            _callback = std::move(cb);
            _expiration = when;
            _interval = interval;
            _is_repeating = (interval > 0);
            setTimer();
        }
        return ret;
    }
    bool Timer::resetTimer(shanchuan::Timestamp newtime)
    {
        bool ret = true;
        if (_is_repeating)
        {
            _expiration = shanchuan::addmsTime(newtime, _interval);
            setTimer();
        }
        else
        {
            _expiration = shanchuan::Timestamp::Invalid();
            ret = false;
        }
        return ret;
    }
    void Timer::handleEvent()
    {
        uint64_t expire_count;
        if (::read(_timerfd, &expire_count, sizeof(expire_count)) != sizeof(expire_count))
        {
            LOG_ERROR << "Failed to read timerfd: " << strerror(errno);
            return;
        }
        LOG_INFO << "expired count=" << (unsigned long long)expire_count;
        if (_callback != nullptr)
        {
            _callback();
        }
    }
    int Timer::getTimerId() const
    {
        return _timerfd;
    }
    bool Timer::isRepeating() const
    {
        return _is_repeating;
    }
    bool Timer::closeTimer()
    {
        bool ret = false;
        if (_timerfd > 0)
        {
            if (::close(_timerfd) == 0)
                ret = true;
            else
                LOG_ERROR << "Failed to close timerfd: " << strerror(errno);
            _timerfd = -1;
            _callback = nullptr;
            _interval = 0;
            _is_repeating = false;
            _expiration = shanchuan::Timestamp::Invalid();
        }
        return ret;
    }
} // namespace shanchuan::scheduled