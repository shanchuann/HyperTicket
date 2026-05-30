#include <functional>
#include <list>
#include <atomic>
#include <thread>
#include <memory>
#include <future>
#include <type_traits>
#include "SyncQueue.hpp"
#include "Logger.hpp"

#ifndef FIXEDTHREADPOOL_HPP
#define FIXEDTHREADPOOL_HPP

namespace shanchuan
{
    class FixedThreadPool
    {
    public:
        using Task = std::function<void(void)>;
    private:
        std::list<std::shared_ptr<std::thread>> _thread_group;
        SyncQueue<Task> _task_queue;
        std::atomic<bool> _is_running;  // 线程池是否正在运行
        std::once_flag _stop_once_flag; // 确保线程池只被停止一次

        void start(int num_threads) {
            _is_running = true;
            for (int i = 0; i < num_threads; ++i) {
                _thread_group.push_back(std::make_shared<std::thread>(&shanchuan::FixedThreadPool::run_in_thread, this));
            }
        }
        void run_in_thread() {
            while (_is_running) {
                Task task;
                if (_task_queue.take(task) == 0) task();
            }
        }
        void stop_thread_group() {
            _task_queue.force_stop();
            _is_running = false;
            for (auto &thread : _thread_group) if (thread->joinable()) thread->join();
            _thread_group.clear();
        }
    public:
        FixedThreadPool(int num_threads = std::thread::hardware_concurrency()) : _task_queue(MaxTaskCount, 1), _is_running(false) {
            start(num_threads);
        }
        ~FixedThreadPool() {
            if (_is_running) stop();
        }
        void stop() {
            std::call_once(_stop_once_flag, [this]() { stop_thread_group(); });
        }
        void add_task(const Task &task) {
            if (_task_queue.put(task) != 0) {
                LOG_ERROR << "[FixedThreadPool] add_task(): Failed to add task to the queue, it may be full or the thread pool is stopping.";
                if (task) task(); // 直接执行任务，避免丢失
            }
        }
        void add_task(Task &&task) {
            if (_task_queue.put(std::forward<Task>(task)) != 0) {
                LOG_ERROR << "[FixedThreadPool] add_task(): Failed to add task to the queue, it may be full or the thread pool is stopping.";
                if (task) task(); // 直接执行任务，避免丢失
            }
        }
        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args) {
            using result_type = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<result_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            auto future = task->get_future();
            Task wrapper = [task]() mutable { (*task)(); };
            // 尝试提交包装后的任务到队列，队列已满或正在停止时，直接执行任务以避免丢失
            if (_task_queue.put(wrapper) != OK) {
                (*task)(); 
            }
            return future;
        }
    };
}
#endif // FIXEDTHREADPOOL_HPP