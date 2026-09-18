#include "AsynLogging.hpp"
#include <algorithm>
#include <stdexcept>

namespace logsys
{
    void AsynLogging::workthreadfunc() {
        latch_.countDown();
        for (;;) {
            std::deque<std::string> buffersToWrite;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                while (running_ && buffers_.empty() && currentBuffer_.empty()) {
                    cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
                }
                if (!currentBuffer_.empty()) {
                    buffers_.push_back(std::move(currentBuffer_));
                    currentBuffer_.reserve(bufferSize_);
                }
                buffersToWrite.swap(buffers_);
                spaceCond_.notify_all();
            }
            {
                std::lock_guard<std::mutex> outputLock(outputMutex_);
                for (const auto &buf : buffersToWrite) output_.append(buf);
            }
            if (!running_ && buffersToWrite.empty()) break;
        }
        output_.flush();
    }
    AsynLogging::AsynLogging(const std::string &basename, const size_t rollSize, int flushInterval,
                             OverflowPolicy overflowPolicy)
        : flushInterval_(std::max(1, flushInterval)),
          bufferSize_(1024 * 4),
          maxBuffers_(16),
          running_(false),
          pthread_{nullptr},
          overflowPolicy_(overflowPolicy),
          output_{basename, LogFileOptions{rollSize, flushInterval, 30, true, 0, 0, 0}},
          latch_(1) {
        currentBuffer_.reserve(bufferSize_);
    }
    AsynLogging::AsynLogging(const std::string &basename, const LogFileOptions &options,
                             OverflowPolicy overflowPolicy)
        : flushInterval_(std::max(1, options.flushInterval)),
          bufferSize_(1024 * 4),
          maxBuffers_(16),
          running_(false),
          pthread_{nullptr},
          overflowPolicy_(overflowPolicy),
          output_{basename, options},
          latch_(1) {
        currentBuffer_.reserve(bufferSize_);
    }
    AsynLogging::AsynLogging(const std::string &basename, const AsynLoggingOptions &options)
        : flushInterval_(std::max(1, options.file.flushInterval)),
          bufferSize_(std::max<std::size_t>(1, options.bufferSize)),
          maxBuffers_(std::max<std::size_t>(1, options.maxBuffers)),
          running_(false),
          pthread_{nullptr},
          overflowPolicy_(options.overflowPolicy),
          output_{basename, options.file},
          latch_(1) {
        currentBuffer_.reserve(bufferSize_);
    }
    AsynLogging::~AsynLogging() { if (running_) stop(); }
    void AsynLogging::append(const std::string &msg) { append(msg.c_str(), msg.size()); }
    void AsynLogging::append(const char *msg, const size_t len) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!running_) {
            lock.unlock();
            std::lock_guard<std::mutex> outputLock(outputMutex_);
            output_.append(msg, len);
            return;
        }
        if (currentBuffer_.size() >= bufferSize_ || currentBuffer_.capacity() - currentBuffer_.size() < len) {
            while (buffers_.size() >= maxBuffers_ && running_) {
                if (overflowPolicy_ == OverflowPolicy::DropNewest) {
                    droppedMessages_.fetch_add(1);
                    return;
                }
                if (overflowPolicy_ == OverflowPolicy::DropOldest) {
                    buffers_.pop_front();
                    droppedMessages_.fetch_add(1);
                    break;
                }
                spaceCond_.wait(lock);
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_.reserve(bufferSize_);
        }
        currentBuffer_.append(msg, len);
        cond_.notify_all();
    }
    void AsynLogging::start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pthread_) throw std::logic_error("AsynLogging cannot be started twice");
        latch_.reset(1);
        running_ = true;
        pthread_.reset(new std::thread(&AsynLogging::workthreadfunc, this));
        latch_.wait();
    }
    void AsynLogging::stop() {
        std::unique_ptr<std::thread> thread;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!pthread_) return;
            running_ = false;
            thread = std::move(pthread_);
        }
        cond_.notify_all();
        spaceCond_.notify_all();
        if (thread->joinable()) thread->join();
    }
    void AsynLogging::flush() {
        std::deque<std::string> bufferToWriter;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!currentBuffer_.empty()) {
                buffers_.push_back(std::move(currentBuffer_));
                currentBuffer_.reserve(bufferSize_);
            }
            bufferToWriter.swap(buffers_);
            spaceCond_.notify_all();
        }
        {
            std::lock_guard<std::mutex> outputLock(outputMutex_);
            for (const auto &buff : bufferToWriter) output_.append(buff);
            output_.flush();
        }
    }
};
