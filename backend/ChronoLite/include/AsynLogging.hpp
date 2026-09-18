#ifndef ASYN_LOGGING_HPP
#define ASYN_LOGGING_HPP

#include <atomic>
#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include "LogFile.hpp"
#include "CountDownLatch.hpp"

namespace logsys
{
    enum class OverflowPolicy { Block, DropNewest, DropOldest };

    struct AsynLoggingOptions {
        LogFileOptions file;
        std::size_t bufferSize = 1024 * 4;
        std::size_t maxBuffers = 16;
        OverflowPolicy overflowPolicy = OverflowPolicy::Block;
    };

    class AsynLogging
    {
    private:
        void workthreadfunc();
        const int flushInterval_;
        const std::size_t bufferSize_;
        const std::size_t maxBuffers_;
        std::atomic<bool> running_;
        std::unique_ptr< std::thread > pthread_;
        std::mutex mutex_;
        std::mutex outputMutex_;
        std::condition_variable cond_;
        std::condition_variable spaceCond_;
        std::string currentBuffer_;
        std::deque<std::string> buffers_;
        const OverflowPolicy overflowPolicy_;
        std::atomic<std::uint64_t> droppedMessages_{0};
        logsys::LogFile output_;
        logsys::CountDownLatch latch_;
    public:
        AsynLogging(const std::string &basename, const size_t rollSize = 1024*128,
                    int flushInterval = 3, OverflowPolicy overflowPolicy = OverflowPolicy::Block);
        AsynLogging(const std::string &basename, const LogFileOptions &options,
                    OverflowPolicy overflowPolicy = OverflowPolicy::Block);
        AsynLogging(const std::string &basename, const AsynLoggingOptions &options);
        ~AsynLogging();
        void append(const std::string &msg);
        void append(const char *msg, const size_t len);
        void flush();
        void start();
        void stop();
        std::uint64_t droppedMessages() const { return droppedMessages_.load(); }
    };
} // namespace logsys
#endif // ASYN_LOGGING_HPP
