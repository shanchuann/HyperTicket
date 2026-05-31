#include "AsynLogging.hpp"

namespace logsys
{
    const size_t BufMaxLen = 1024 * 4;
    const size_t BufQueueSize = 16;
    void AsynLogging::workthreadfunc() {
        std::vector<std::string> buffersToWrite;
        latch_.countDown();
        while (running_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                while(buffers_.empty() && running_)
                {
                    cond_.wait_for(lock, std::chrono::seconds(1));
                }
                buffers_.push_back(std::move(currentBuffer_));
                currentBuffer_.reserve(BufMaxLen);
                buffersToWrite.swap(buffers_);
                buffers_.reserve(BufQueueSize);
            }
            if (buffersToWrite.size() > 25) {
                fprintf(stderr, "Dropped log message at larger buffers \n");
                buffersToWrite.erase(buffersToWrite.begin()+ 2, buffersToWrite.end());
            }
            for(const auto &buf: buffersToWrite) { output_.append(buf); }
            buffersToWrite.clear();
        }
        output_.flush();
    }
    AsynLogging::AsynLogging(const std::string &basename, const size_t rollSize, int flushInterval)
        : flushInterval_(flushInterval),
          running_(false),
          rollSize_(rollSize),
          pthread_{nullptr},
          output_{basename, rollSize},
          latch_(1) {
        currentBuffer_.reserve(BufMaxLen);
        buffers_.reserve(BufQueueSize);
    }
    AsynLogging::~AsynLogging() { if (running_) stop(); }
    void AsynLogging::append(const std::string &msg) { append(msg.c_str(), msg.size()); }
    void AsynLogging::append(const char *msg, const size_t len) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (currentBuffer_.size() >= BufMaxLen || currentBuffer_.capacity() - currentBuffer_.size() < len) {
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_.reserve(BufMaxLen);
        }
        currentBuffer_.append(msg, len);
        cond_.notify_all();
    }
    void AsynLogging::start() {
        running_ = true;
        pthread_.reset(new std::thread(&AsynLogging::workthreadfunc, this));
        latch_.wait();
    }
    void AsynLogging::stop() {
        running_ = false;
        cond_.notify_all();
        pthread_->join();
    }
    void AsynLogging::flush() {
        std::vector<std::string> bufferToWriter;
        std::unique_lock<std::mutex> lock(mutex_);
        buffers_.push_back(std::move(currentBuffer_));
        bufferToWriter.swap(buffers_);
        for(const auto &buff:bufferToWriter) { output_.append(buff); }
        output_.flush();
        bufferToWriter.clear();
    }
};