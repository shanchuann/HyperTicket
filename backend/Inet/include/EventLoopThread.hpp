

#ifndef EVENT_LOOP_THREAD_HPP
#define EVENT_LOOP_THREAD_HPP
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
using namespace std;

namespace shanchuan
{
    class EventLoop;
    class EventLoopThread
    {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;
        EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback());
        ~EventLoopThread();
        EventLoop *startLoop();

    private:
        void threadFunc();

        EventLoop *loop_;
        bool exiting_;
        std::unique_ptr<std::thread> thread_;
        std::mutex mutex_;
        std::condition_variable cond_;
        ThreadInitCallback callback_;
    };

}

#endif