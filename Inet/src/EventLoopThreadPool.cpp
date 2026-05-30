//C API
#include<assert.h>
//
#include "EventLoop.hpp"
#include "EventLoopThread.hpp"
#include "EventLoopThreadPool.hpp"

namespace shanchuan
{
    // class EventLoopThreadPool
    // using ThreadInitCallback = std::function<void(EventLoop *)>;
    // EventLoop *baseLoop_;                                   // 主线程loop
    // bool started_;                                          // 标记当前状态 即IO线程是否开始运行
    // int numThreads_;                                        // 线程池中线程的数量
    // int next_;                                              // 负载均衡用
    // std::vector<std::unique_ptr<EventLoopThread>> threads_; // 创建事件的线程
    // std::vector<EventLoop *> loops_;
    //  事件线程里面EventLoop的指针，
    //  每个EventLoopThread线程对应的EventLoop保存在loops_中

    EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop)
        :baseLoop_(baseLoop),
        started_(false),
        numThreads_(0),
        next_(0)

    {
        LOG_TRACE<<"Create EventLoopThreadPool"<<numThreads_;
    }
    EventLoopThreadPool::~EventLoopThreadPool()
    {
        //不要删除循环，它是栈变量
    }
    void EventLoopThreadPool::setThreadNum(int numThreads)
    {
        numThreads_ = numThreads;
    }
    void EventLoopThreadPool::start(const ThreadInitCallback &cb )
    {
        LOG_TRACE<<"Start "<<numThreads_;
        assert(!started_);
        baseLoop_->assertInLoopThread();
        started_ = true;
        for(int i = 0 ;i<numThreads_;++i)
        {
            EventLoopThread * t = new EventLoopThread(cb);
            threads_.push_back(std::unique_ptr<EventLoopThread>(t));
            loops_.push_back(t->startLoop());
        }
        if(0 == numThreads_ && cb)
        {
            LOG_TRACE<<"call cb ";
            cb(baseLoop_);
        } 
    }
    EventLoop *EventLoopThreadPool::getNextLoop()
    {
        LOG_TRACE<<"Next: "<<next_;
        LOG_TRACE<<"loops_empty(): "<<loops_.empty();
        baseLoop_->assertInLoopThread();
        assert(started_);
        EventLoop *loop = baseLoop_;
        if(!loops_.empty())
        {
            loop = loops_[next_];
            ++next_;
            if(static_cast<size_t>(next_) >= loops_.size())
            {
                next_ = 0;
            }
        }
        return loop;
    }
    EventLoop *EventLoopThreadPool::getLoopForHash(size_t hashCode)
    {
        baseLoop_->assertInLoopThread();
        EventLoop * loop = baseLoop_;
        if(!loops_.empty())
        {
            loop = loops_[hashCode % loops_.size()];
        }
        return loop;
    }
    std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
    {
        baseLoop_->assertInLoopThread();
        assert(started_);
        if(loops_.empty())
        {
            return std::vector<EventLoop*>(1,baseLoop_);
        }
        else
        {
            return loops_;
        }
    }
    bool EventLoopThreadPool::started() const
    {
         return started_;
    }

} // namespace shanchuan