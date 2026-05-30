

#include<assert.h>

#include"EventLoop.hpp"
#include"EventLoopThread.hpp"

namespace shanchuan
{
    EventLoopThread::EventLoopThread(const ThreadInitCallback &cb)
        :loop_(nullptr),
        exiting_(false),
        thread_(nullptr),
        mutex_{},
        cond_{},
        callback_(cb)
    {
        LOG_TRACE<<"Create ";
    }

    EventLoopThread::~EventLoopThread()
    {
        LOG_TRACE<<"distroy";
        exiting_ = true;
        if(nullptr != loop_)
        {
            loop_->quit();
            thread_->join();
        }
    }
    EventLoop * EventLoopThread::startLoop()
    {
        LOG_TRACE<<"startLoop";
        assert(!thread_);
        thread_.reset(new thread(std::bind(&EventLoopThread::threadFunc,this)));
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while(nullptr == loop_)
            {
                cond_.wait(lock);
            }
        }
        return loop_;
    }
    void EventLoopThread::threadFunc()
    {
        LOG_TRACE<<"begin thread func";
        EventLoop loop;
        if(callback_)
        {
            callback_(&loop);
        }
        {
            std::unique_lock<std::mutex> lock(mutex_);
            loop_ = &loop;
            cond_.notify_all();
        }
        loop.loop();
        loop_ = nullptr;
    }
} // namespace shanchuan
