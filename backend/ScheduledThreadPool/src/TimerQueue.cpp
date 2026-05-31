#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "../include/Timer.hpp"
#include "../include/TimerQueue.hpp"

namespace shanchuan::scheduled
{
    void TimerQueue::loop()
    {
        while(!_is_stop){
            int nfds = ::epoll_wait(_epollfd, _events.data(), _events.size(), _timeout);
            if(nfds < 0)
            {
                if(errno == EINTR)
                {
                    continue; // 被信号中断，继续等待
                }
                LOG_ERROR << "Failed to wait on epoll: " << strerror(errno);
                break;
            }
            for(int i = 0; i < nfds; ++i)
            {
                int fd = _events[i].data.fd;
                Timer *timer = nullptr;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    auto it = _timers.find(fd);
                    if(it != _timers.end())
                        timer = it->second;
                }
                if(timer)
                {
                    timer->handleEvent();
                    if(!timer->isRepeating())
                    {
                        removeTimer({fd, timer});
                    }
                }
                else
                {
                    LOG_ERROR << "Received event for unknown timer fd: " << fd;
                }
            }
            if(nfds>= _events.size())
            {
                _events.resize(_events.size() * 2); // 扩容事件数组
            }
        }
    }
    void TimerQueue::stopQueue()
    {
        _is_stop = true;
        if(_worderThread.joinable())
        {
            _worderThread.join();
        }
        std::lock_guard<std::mutex> lock(_mutex);
        for(auto &pair : _timers)
        {
            pair.second->closeTimer();
            delete pair.second;
        }
        _timers.clear();
        if(_epollfd >= 0)
        {
            ::close(_epollfd);
            _epollfd = -1;
            LOG_TRACE << "TimerQueue stopped and cleaned up";
        }
    }
    TimerQueue::TimerQueue(int timeout) : _epollfd(-1), _timeout(timeout), _is_stop(true)
    {
        _events.resize(eventsize);
        init();
    }
    TimerQueue::~TimerQueue()
    {
        stop();
    }
    void TimerQueue::init()
    {
        _epollfd = ::epoll_create1(EPOLL_CLOEXEC);
        if (_epollfd < 0)
        {
            LOG_ERROR << "Failed to create epoll instance: " << strerror(errno);
            return;
        }
        try{
            _worderThread = std::thread(&TimerQueue::loop, this);
            _is_stop = false;
        }
        catch(const std::exception& e)
        {
            LOG_ERROR << "Failed to start worker thread: " << e.what();
            _is_stop = true;
            close(_epollfd);
            _epollfd = -1;
        }
    }
    std::pair<int, Timer *> TimerQueue::addTimer(timerCallBack cb, const shanchuan::Timestamp &when, size_t interval)
    {
        Timer *timer = new Timer();
        if(!timer->init(std::move(cb), when, interval))
        {
            delete timer;
            return {-1, nullptr};
        }
        if (!timer->setTimer())
        {
            LOG_ERROR << "Failed to arm timerfd for timer";
            timer->closeTimer();
            delete timer;
            return {-1, nullptr};
        }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = timer->getTimerId();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(::epoll_ctl(_epollfd, EPOLL_CTL_ADD, timer->getTimerId(), &ev) < 0)
            {
                LOG_ERROR << "Failed to add timerfd to epoll: " << strerror(errno);
                timer->closeTimer();
                delete timer;
                return {-1, nullptr};
            }
            _timers[timer->getTimerId()] = timer;
        }
        LOG_INFO << "added timer: fd = " << timer->getTimerId();
        return {timer->getTimerId(), timer};
    }
    bool TimerQueue::removeTimer(std::pair<int, Timer *> timer)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        int fd = timer.first;
        auto it = _timers.find(fd);
        if(it != _timers.end())
        {
            Timer *t = it->second;
            ::epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, nullptr);
            _timers.erase(it);
            t->closeTimer();
            delete t;
            LOG_INFO << "Removed timer: fd = " << fd;
            return true;
        }
        return false;
    }
    void TimerQueue::cancel(timerId id){
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _timers.find(id.first);
        if(it != _timers.end())
        {
            Timer *t = it->second;
            _timers.erase(it);
            LOG_INFO << "Cancelled timer: fd = " << id.first;
            t->closeTimer();
            delete t;
        }
    }
    void TimerQueue::stop()
    {
        std::call_once(_flag,&TimerQueue::stopQueue, this);
    }
} // namespace shanchuan::scheduled
