#include "Logger.hpp"
#include "EventLoop.hpp"
#include "Timer.hpp"
#include "TimerId.hpp"
#include "TimerQueue.hpp"
#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

namespace shanchuan
{
    int createTimerfd()
    {
        // 创建定时器文件描述符的系统调用函数
        int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (timerfd < 0)
        {
            LOG_SYSFATAL << "Failed in timerfd_create";
        }
        return timerfd;
    }
    // 从现在起还有多少时间
    struct timespec howMuchTimeFromNow(shanchuan::Timestamp when)
    {
        int64_t microseconds = when.getMicroSec() - shanchuan::Timestamp::Now().getMicroSec();
        if (microseconds < 100)
        {
            microseconds = 100;
        }
        struct timespec ts;
        ts.tv_sec = static_cast<time_t>(microseconds / shanchuan::Timestamp::kMicroSecPerSecond);
        ts.tv_nsec = static_cast<long>((microseconds % shanchuan::Timestamp::kMicroSecPerSecond) * 1000);
        return ts;
    }

    void readTimerfd(int timerfd, shanchuan::Timestamp now)
    {
        LOG_TRACE<<"now: "<<now.toFormattedString();
        uint64_t howmany = 0;
        ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
        LOG_TRACE << "TimerQueue::handleRead()" << howmany 
                  << " at " << now.toFormattedString();
        if (n != sizeof(howmany))
        {
            LOG_ERROR << "TimerQueue::handleRead() reads " << n 
            << "bytes instead of 8";
        }
    }

    //调用linux系统函数，重新设置下到期时间
    void resetTimerfd(int timerfd, shanchuan::Timestamp expiration)
    {
        LOG_TRACE<<"expiration: "<<expiration.toFormattedString();
        struct itimerspec newValue;
        struct itimerspec oldValue;
        memset(&newValue, 0, sizeof(newValue));
        memset(&oldValue, 0, sizeof(oldValue));
        newValue.it_value = howMuchTimeFromNow(expiration);
        int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
        if (ret == -1) LOG_SYSERR << "timerfd_seettime()";
    }
    void TimerQueue::addTimerInLoop(Timer *timer)
    {
        LOG_TRACE<<"timer: "<<timer->expiration().toFormattedString();
        //判断是否在当前线程中
        loop_->assertInLoopThread();
        bool earliestChanged = insert(timer);
        // 返回true，说明timer被添加到set的顶部，作为新的根节点，需要更新timerfd的激活时间 
        if (earliestChanged)
        {
            //如果新入队的定时器是队列里最早的，重新设置定时器到期触发时间
            resetTimerfd(timerfd_, timer->expiration());
        }
    }
    void TimerQueue::cancelInLoop(TimerId timerId)
    {
        LOG_TRACE<<"BEGIN";
        LOG_TRACE<<"timerId: "<<timerId.timer_->expiration().toFormattedString();
        LOG_TRACE<<"TimerId: "<<timerId.sequence_;
        loop_->assertInLoopThread();
        assert(timers_.size() == activeTimers_.size());
        ActiveTimer timer(timerId.timer_, timerId.sequence_);
        ActiveTimerSet::iterator it = activeTimers_.find(timer);
        if (it != activeTimers_.end())
        {
            size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
            assert(n == 1);
            delete it->first;
            activeTimers_.erase(it);
        }
        else if (callingExpiredTimers_)
        {
            cancelingTimers_.insert(timer);
        }
        assert(timers_.size() == activeTimers_.size());
    }

    void TimerQueue::handleRead()
    {
        LOG_TRACE<<"handleRead BEGIN";
        loop_->assertInLoopThread();
        shanchuan::Timestamp now(Timestamp::Now());
        //要读走这个事件，不然会一直触发
        readTimerfd(timerfd_, now);
        std::vector<Entry> expired = getExpired(now);
        //获取所有到期的定时器
        callingExpiredTimers_ = true;// 处于定时器处理状态中
        cancelingTimers_.clear();
        //执行每个到期的定时器方法
        LOG_TRACE<<"expired size(): "<<expired.size();
        for (const Entry &it : expired)
        {
            it.second->run();
        }
        callingExpiredTimers_ = false;
        //重置过期定时器状态，如果是重复执行定时器就再入队，否则删除
        reset(expired, now);
    }
    //获取now之前的过期定时器
    std::vector<typename TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
    {
        LOG_TRACE<<"now: "<<now.toFormattedString();
        assert(timers_.size() == activeTimers_.size());
        std::vector<typename TimerQueue::Entry> expired;
        // 哨兵
        Entry sentry(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
        TimerList::iterator end = timers_.lower_bound(sentry);
        //这里用到了std::set::lower_bound（寻找第一个大于等于Value的值），
        //这里的Timer*是UINTPTR_MAX，所以返回的是一个大于Value的值，
        //即第一个未到期的Timer的迭代器
        assert(end == timers_.end() || now < end->first);
        // 拷贝出所有已过期的定时器（截止 now）
        std::copy(timers_.begin(), end, back_inserter(expired));
        // 从定时器队列中清除这些已过期的定时器
        timers_.erase(timers_.begin(), end);
        for (const Entry &it : expired)
        {
            ActiveTimer timer(it.second, it.second->sequence());
            size_t n = activeTimers_.erase(timer);
            assert(n == 1);
        }
        assert(timers_.size() == activeTimers_.size());
        return expired;
    }
    void TimerQueue::reset(const std::vector<typename TimerQueue::Entry> &expired, Timestamp now) {
        LOG_TRACE<<"now"<<now.toFormattedString();
        LOG_TRACE<<"expired size: "<<expired.size();
        shanchuan::Timestamp nextExpire;
        for (const Entry &it : expired) {
            ActiveTimer timer(it.second, it.second->sequence());
            if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end()) {
                it.second->restart(now);
                insert(it.second);
            }
            else delete it.second; // 非重复定时器，或已取消的定时器，直接删除
        }
        if (!timers_.empty())
        {
            nextExpire = timers_.begin()->second->expiration();
        }
        // 如果 nextExpire 有效（非无效时间戳），则重置 timerfd
        if (nextExpire.valid())
        {
            resetTimerfd(timerfd_, nextExpire);
        }
    }
    bool TimerQueue::insert(Timer *timer)
    {
        LOG_TRACE<<"timer.sequence :"<<timer->sequence();
        loop_->assertInLoopThread();
        assert(timers_.size() == activeTimers_.size());
        // timers_和activeTimers_存着同样的定时器列表，个数是一样的
        bool earliestChanged = false;
        Timestamp when = timer->expiration();
        TimerList::iterator it = timers_.begin();
        if (it == timers_.end() || when < it->first)
        {
            earliestChanged = true;
        }
        //timers_是按过期时间有序排列的，最早到期的在前面
        //新插入的时间和队列中最早到期时间比，判断新插入时间是否更早
        {
            //将时间保存入队，std::set自动保存有序
            std::pair<TimerList::iterator, bool> result =
                timers_.insert(Entry(when, timer));
            assert(result.second);
        }
        {
            std::pair<ActiveTimerSet::iterator, bool> result =
                activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
            assert(result.second);
        }
        assert(timers_.size() == activeTimers_.size());
        return earliestChanged;
    }

    TimerQueue::TimerQueue(EventLoop *loop)
        : loop_(loop),
          timerfd_(createTimerfd()),
          timerfdChannel_(loop, timerfd_),
          timers_(),
          callingExpiredTimers_(false)
    {
        LOG_TRACE<<"Create";
        timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
        timerfdChannel_.enableReading(); 
    }
    TimerQueue::~TimerQueue()
    {
        timerfdChannel_.disableAll();
        timerfdChannel_.remove();
        ::close(timerfd_);
        for (const Entry &timer : timers_)
        {
            delete timer.second;
        }
        LOG_TRACE<<"Destroy";
    }

    ///cb 定时器到期执行函数，when 定时器到期时间，interval非零表示重复定时器
    TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
    {
        LOG_TRACE<<"when"<<when.toFormattedString();
        LOG_TRACE<<"interval: "<<internal;
        //实例化一个定期器类
        Timer *timer = new Timer(std::move(cb), when, interval);
        //将addTimerInLoop方法放到EventLoop中执行，若用户在当前IO线程，
        //回调则同步执行，否则将方法加入到队列，待IO线程被唤醒之后再来调用此回调。
        loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
        //实例化一个定时器和序列号封装的TimerId进行返回，用于用户取消定时器
        return TimerId(timer, timer->sequence());
    }
    void TimerQueue::cancel(TimerId timerId)
    {
        LOG_TRACE<<"timerId.sequence_"<<timerId.sequence_;
        loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
    }

} // namespace shanchuan

// int timerfd_create(int clockid, int flags);
// clockid: 
// 指定要使用的时钟源，常用的时钟源有以下几种：
// // CLOCK_REALTIME: 系统的实时时钟，这个时钟可以被系统管理员调整，因此它的值可能会发生跳跃。
// // CLOCK_MONOTONIC: 单调递增的时钟，从系统启动开始计时，不受系统时间调整的影响，适合用于测量时间间隔。
// flags:
// 这是一个位掩码，用于设置定时器文件描述符的一些属性，常用的标志位有：
// // TFD_NONBLOCK: 将创建的文件描述符设置为非阻塞模式。在非阻塞模式下，如果定时器事件还未发生，
// // 对该文件描述符的读取操作将立即返回一个错误（EAGAIN 或 EWOULDBLOCK），而不会阻塞线程。
// // TFD_CLOEXEC：设置文件描述符的 close-on-exec 标志。当程序执行 exec 系列函数时，
// // 这个文件描述符会自动关闭，避免在新程序中继续使用。
// 在代码中，TFD_NONBLOCK | TFD_CLOEXEC 表示同时设置这两个标志，即创建的定时器文件描述符是非阻塞的，
// // 并且在执行 exec 时会自动关闭。
// // 如果函数调用成功，timerfd_create 会返回一个新的文件描述符，用于后续对定时器的操作（如设置定时器参数、读取定时器事件等）。
// // 如果调用失败，函数会返回 -1，并设置 errno 来指示具体的错误原因。

// struct itimerspec {
//     struct timespec it_interval;  /* 定时器触发的间隔时间 */
//     struct timespec it_value;     /* 定时器首次触发的时间 */
// };
// struct timespec it_interval
// // 这个成员指定了定时器在首次触发之后，后续每次触发的时间间隔。如果 it_interval 的秒数和纳秒数都为 0，
// 那么定时器只会触发一次，即不会重复触发。
// 例如，如果 it_interval.tv_sec = 1 且 it_interval.tv_nsec = 0，
// 表示定时器首次触发后，每隔 1 秒触发一次。
// struct timespec it_value
// 该成员指定了定时器首次触发的时间，即从调用 timerfd_settime 或 timer_settime 函数开始，
// 经过 it_value 所指定的时间后，定时器会首次触发。
// 同样使用 struct timespec 结构体来表示时间。
// // 如果 it_value 的秒数和纳秒数都为 0，那么定时器会立即停止（如果已经启动）或者不会启动。