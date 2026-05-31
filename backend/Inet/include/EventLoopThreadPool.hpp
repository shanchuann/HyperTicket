

#ifndef EVENT_LOOP_THREAD_POOL_HPP
#define EVENT_LOOP_THREAD_POOL_HPP

#include <vector>
#include <functional>
#include <memory>
using namespace std;

namespace shanchuan
{
    class EventLoop;
    class EventLoopThread;
    class EventLoopThreadPool
    {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;
    private:
        EventLoop *baseLoop_;                                   // 主线程loop
        bool started_;                                          // 标记当前状态 即IO线程是否开始运行
        int numThreads_;                                        // 线程池中线程的数量
        int next_;                                              // 负载均衡用
        std::vector<std::unique_ptr<EventLoopThread>> threads_; // 创建事件的线程
        std::vector<EventLoop *> loops_;
        // 事件线程里面EventLoop的指针，
        // 每个EventLoopThread线程对应的EventLoop保存在loops_中
    public:
        EventLoopThreadPool(EventLoop *baseLoop);
        ~EventLoopThreadPool();
        void setThreadNum(int numThreads);//设置线程的数量
        //启动线程池,实际上创建numThreads个线程,并让每个eventloopthread调用startLoop()
        void start(const ThreadInitCallback &cb = ThreadInitCallback());
        EventLoop *getNextLoop();// 以轮询的方式分配channel给subloop
        EventLoop *getLoopForHash(size_t hashCode);//采用hash方式分配EventLoop
        std::vector<EventLoop *> getAllLoops();
        bool started() const; //获取线程状态
    };

} // namespace shanchuan

#endif