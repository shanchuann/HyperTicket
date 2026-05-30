#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP

#include <mutex>
#include <condition_variable>

namespace shanchuan
{
    // A simple counting semaphore (pre-C++20 substitute for std::counting_semaphore).
    class Semaphore
    {
    public:
        explicit Semaphore(int count = 0) : count_(count) {}

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
} // namespace shanchuan
#endif // SEMAPHORE_HPP
