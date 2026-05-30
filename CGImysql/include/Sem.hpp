#ifndef SEM_HPP
#define SEM_HPP

#include <mutex>
#include <condition_variable>

class sem
{
public:
    explicit sem(int count = 0) : count_(count) {}
    void reset(int count) {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ = count;
        condition_.notify_all();
    }
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this]() { return count_ > 0; });
        --count_;
    }
    void post() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    int count_;
};
#endif // SEM_HPP
