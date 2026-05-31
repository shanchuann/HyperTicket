#include <sys/epoll.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "Logger.hpp"
#include "Timer.hpp"
#include "Notcopy.hpp"

#ifndef SCHEDULED_TIMERQUEUE_HPP
#define SCHEDULED_TIMERQUEUE_HPP

namespace shanchuan::scheduled
{
    class TimerQueue: public Notcopy
    {
    private:
        static const int eventsize = 16;
    private:
        int _epollfd;
        int _timeout; // epoll_wait的超时时间，单位为毫秒，-1表示无限等待
        std::vector<struct epoll_event> _events;
        std::unordered_map<int, Timer *> _timers;
        std::mutex _mutex;    // 保护 _timers 和 _epollfd 的并发访问
        std::once_flag _flag; // 确保TimerQueue的Stop()只启动一次
        std::atomic<bool> _is_stop; // true表示停止TimerQueue，false表示运行中
        std::thread _worderThread;
        void loop();
        void init();
        void stopQueue();
    public:
        TimerQueue(int timeout = -1);
        ~TimerQueue();
        scheduled::timerId addTimer(timerCallBack cb, const shanchuan::Timestamp &when, size_t interval);
        std::pair<int, Timer *> addTimer(timerCallBack cb, size_t when, size_t interval);
        bool removeTimer(std::pair<int, Timer *> timer);
        void cancel(timerId id);
        void stop();
    };
    } // namespace shanchuan::scheduled
#endif // SCHEDULED_TIMERQUEUE_HPP
