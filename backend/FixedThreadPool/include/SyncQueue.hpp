#include <deque>
#include <chrono>
#include <mutex>
#include <condition_variable>

#ifndef SYNCQUEUE_HPP
#define SYNCQUEUE_HPP

static const int MaxTaskCount = 100;

namespace shanchuan
{
    enum class QueueResult
    {
        Ok = 0,
        Full,      // 队列已满，put 失败
        Stopped,   // 队列已停止
        Timeout,   // 等待超时
    };
    template <typename T>
    class SyncQueue
    {
    private:
        std::deque<T> _task_queue;
        mutable std::mutex _mutex;
        std::condition_variable _cv_not_empty;  // 当队列不为空时通知消费者线程
        std::condition_variable _cv_not_full;   // 当队列不满时通知生产者线程
        std::condition_variable _cv_wait_empty; // 当队列为空时通知等待停止的线程
        size_t _max_capacity;                   // 队列的最大容量，超过该容量时生产者线程将被阻塞
        bool _stop_flag;                        // true表示线程池正在停止，false表示线程池正常运行
        int _wait_timeout = 1;                  // 等待超时时间，单位为秒

        bool is_full() const {
            bool full = _task_queue.size() >= _max_capacity;
            return full;
        }
        bool is_empty() const {
            bool empty = _task_queue.empty();
            return empty;
        }
        template <typename F>
        QueueResult add(F &&task)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            bool tag = _cv_not_full.wait_for(lock, std::chrono::seconds(_wait_timeout), [this]() { return !is_full() || _stop_flag; });
            if (!tag) return QueueResult::Full;
            if (_stop_flag) return QueueResult::Stopped;
            _task_queue.emplace_back(std::forward<F>(task));
            _cv_not_empty.notify_all();
            return QueueResult::Ok;
        }

    public:
        SyncQueue(size_t max_capacity = MaxTaskCount,int wait_timeout = 1) : _max_capacity(max_capacity), _stop_flag(false), _wait_timeout(wait_timeout) {}
        ~SyncQueue() {
            if (!_stop_flag) force_stop();
        }
        QueueResult put(const T &task) {
            return add(task);
        }
        QueueResult put(T &&task) {
            return add(std::forward<T>(task));
        }

        /*
        void take(T *task) {
            assert(task != nullptr);
            std::unique_lock<std::mutex> lock(_mutex);
            while (is_empty() && !_stop_flag) _cv_not_empty.wait(lock);
            if (_stop_flag) throw std::runtime_error("ThreadPool is stopping, cannot take tasks.");
            *task = std::move(_task_queue.front());
            _task_queue.pop_front();
            _cv_not_full.notify_all();
            _cv_wait_empty.notify_one();
        }
        void take(std::deque<T> *task_list) {
            assert(task_list != nullptr);
            std::unique_lock<std::mutex> lock(_mutex);
            while (is_empty() && !_stop_flag) {
                _cv_not_empty.wait(lock);
            }
            if (_stop_flag) return; // 线程池正在停止且队列已空，直接返回
            *task_list = std::move(_task_queue);
            _task_queue.clear();
            _cv_not_full.notify_all();
            _cv_wait_empty.notify_one();
        }
        */
        
        QueueResult take(T&task) {
            std::unique_lock<std::mutex> lock(_mutex);
            bool tag = _cv_not_empty.wait_for(lock, std::chrono::seconds(_wait_timeout), [this]() { return !is_empty() || _stop_flag; });
            if (!tag) return QueueResult::Timeout;
            if(_stop_flag) return QueueResult::Stopped;
            task = std::move(_task_queue.front());
            _task_queue.pop_front();
            _cv_not_full.notify_all();
            return QueueResult::Ok;
        }
        QueueResult take(std::deque<T> &task_list) {
            std::unique_lock<std::mutex> lock(_mutex);
            bool tag = _cv_not_empty.wait_for(lock, std::chrono::seconds(_wait_timeout), [this]() { return !is_empty() || _stop_flag; });
            if (!tag) return QueueResult::Timeout;
            if(_stop_flag) return QueueResult::Stopped;
            task_list = std::move(_task_queue);
            _task_queue.clear();
            _cv_not_full.notify_all();
            return QueueResult::Ok;
        }
        void force_stop() {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _stop_flag = true;
            }
            _cv_not_empty.notify_all();
            _cv_not_full.notify_all();
        }

        /*
        void wait_stop() {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                while (!_task_queue.empty()) {
                    _cv_wait_empty.wait(lock);
                }
            }
            _cv_not_empty.notify_all();
            _cv_not_full.notify_all();
        }
        */

        bool empty() const {
            std::lock_guard<std::mutex> lock(_mutex);
            return _task_queue.empty();
        }
        bool full() const {
            std::lock_guard<std::mutex> lock(_mutex);
            return _task_queue.size() >= _max_capacity;
        }
        size_t size() const {
            std::lock_guard<std::mutex> lock(_mutex);
            return _task_queue.size();
        }
        size_t count() const {
            std::lock_guard<std::mutex> lock(_mutex);
            return _task_queue.size();
        }
    };
}
#endif // SYNCQUEUE_HPP
