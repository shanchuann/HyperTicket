#include <deque>
#include <chrono>
#include <mutex>
#include <condition_variable>

#ifndef SYNCQUEUE_HPP
#define SYNCQUEUE_HPP

#define OK 0
#define QUEUE_FULL 1
#define QUEUE_STOPPED 2
static const int MaxTaskCount = 100;

namespace shanchuan
{
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
        int add(F &&task)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            /*
            // 生产者线程在添加任务时，如果队列已满，则等待直到队列不满或线程池正在停止
            while(!_stop_flag && is_full()){
                _cv_not_full.wait(lock);
            }
            // lambda
            _cv_not_full.wait(lock, [this]() { return !is_full() || _stop_flag; });
            */

            /*
            // 生产者线程在添加任务时，如果队列已满，则等待直到队列不满或线程池正在停止，等待时设置超时时间，避免死锁
            while(!_stop_flag && is_full()){
                if(std::cv_status::timeout == _cv_not_full.wait_for(lock, std::chrono::seconds(_wait_timeout))){
                    return QUEUE_FULL;
                }
            }
            */

            // 生产者线程在添加任务时，如果队列已满，则等待直到队列不满或线程池正在停止，等待时设置超时时间，避免死锁
            bool tag = _cv_not_full.wait_for(lock, std::chrono::seconds(_wait_timeout), [this]() { return !is_full() || _stop_flag; });
            if (!tag) return QUEUE_FULL;
            if (_stop_flag) return QUEUE_STOPPED;
            _task_queue.emplace_back(std::forward<F>(task));
            _cv_not_empty.notify_all();
            return OK;
        }

    public:
        SyncQueue(size_t max_capacity = MaxTaskCount,int wait_timeout = 1) : _max_capacity(max_capacity), _stop_flag(false), _wait_timeout(wait_timeout) {}
        ~SyncQueue() {
            if (!_stop_flag) force_stop();
        }
        int put(const T &task) {
            return add(task);
        }
        int put(T &&task) {
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
        
        int take(T&task) {
            std::unique_lock<std::mutex> lock(_mutex);
            bool tag = _cv_not_empty.wait_for(lock, std::chrono::seconds(_wait_timeout), [this]() { return !is_empty() || _stop_flag; });
            if (!tag) return QUEUE_FULL; // 等待超时，返回队列满的状态
            if(_stop_flag) return QUEUE_STOPPED;
            task = std::move(_task_queue.front());
            _task_queue.pop_front();
            _cv_not_full.notify_all();
            return OK;
        }
        int take(std::deque<T> &task_list) {
            std::unique_lock<std::mutex> lock(_mutex);
            bool tag = _cv_not_empty.wait_for(lock, std::chrono::seconds(_wait_timeout), [this]() { return !is_empty() || _stop_flag; });
            if (!tag) return QUEUE_FULL; // 等待超时，返回队列满的状态
            if(_stop_flag) return QUEUE_STOPPED;
            task_list = std::move(_task_queue);
            _task_queue.clear();
            _cv_not_full.notify_all();
            return OK;
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
            return _task_queue.size();
        }
    };
}
#endif // SYNCQUEUE_HPP
