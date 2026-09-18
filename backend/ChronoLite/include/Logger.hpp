#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "LogMessage.hpp"
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <utility>

#define LOG_TRACE    \
    if (logsys::shouldLog(logsys::LOG_LEVEL::TRACE, logsys::Logger::getLogLevel())) \
    logsys::Logger(logsys::LOG_LEVEL::TRACE, __FILE__, __func__, __LINE__).stream()
#define LOG_DEBUG    \
    if (logsys::shouldLog(logsys::LOG_LEVEL::DEBUG, logsys::Logger::getLogLevel())) \
    logsys::Logger(logsys::LOG_LEVEL::DEBUG, __FILE__, __func__, __LINE__).stream()
#define LOG_INFO     \
    if (logsys::shouldLog(logsys::LOG_LEVEL::INFO, logsys::Logger::getLogLevel())) \
    logsys::Logger(logsys::LOG_LEVEL::INFO , __FILE__, __func__, __LINE__).stream()
#define LOG_WARN     \
    if (logsys::shouldLog(logsys::LOG_LEVEL::WARN, logsys::Logger::getLogLevel())) \
    logsys::Logger(logsys::LOG_LEVEL::WARN , __FILE__, __func__, __LINE__).stream()
#define LOG_ERROR    \
    if (logsys::shouldLog(logsys::LOG_LEVEL::ERROR, logsys::Logger::getLogLevel())) \
    logsys::Logger(logsys::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__).stream()
#define LOG_FATAL    \
    if (logsys::shouldLog(logsys::LOG_LEVEL::FATAL, logsys::Logger::getLogLevel())) \
    logsys::Logger(logsys::LOG_LEVEL::FATAL, __FILE__, __func__, __LINE__).stream()
#define LOG_SYSERR   \
    logsys::Logger(logsys::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__).stream()
#define LOG_SYSFATAL \
    logsys::Logger(logsys::LOG_LEVEL::FATAL, __FILE__, __func__, __LINE__).stream()
#define LOG_AUDIT \
    if (logsys::Logger::shouldLog(logsys::LOG_LEVEL::INFO)) \
    logsys::Logger(logsys::LOG_LEVEL::INFO, __FILE__, __func__, __LINE__, logsys::LogCategory::Audit).stream()
#define LOG_USER_ACTION \
    if (logsys::Logger::shouldLog(logsys::LOG_LEVEL::INFO)) \
    logsys::Logger(logsys::LOG_LEVEL::INFO, __FILE__, __func__, __LINE__, logsys::LogCategory::UserAction).stream()
#define LOG_TRANSACTION \
    if (logsys::Logger::shouldLog(logsys::LOG_LEVEL::INFO)) \
    logsys::Logger(logsys::LOG_LEVEL::INFO, __FILE__, __func__, __LINE__, logsys::LogCategory::Transaction).stream()
#define LOG_TRACEABLE_ERROR \
    if (logsys::Logger::shouldLog(logsys::LOG_LEVEL::ERROR)) \
    logsys::Logger(logsys::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__, logsys::LogCategory::TraceableError).stream()

namespace logsys
{
    class Logger
    {
    public:
        using OutputFun = std::function<void(const std::string &)>;
        using FlushFun  = std::function<void(void)>;
        using FatalFun = std::function<void(void)>;
        static OutputFun s_output_;
        static FlushFun  s_flush_;
        static void SetOutput(OutputFun fun);
        static void SetOuput(OutputFun fun);
        static void SetFlush(FlushFun  fun);
        static void SetFlushOnEachMessage(bool enabled);
        static void SetFatalHandler(std::function<void()> handler);
        // Compatibility with HyperTicket integrations using the original API.
        static void SetFatal(FatalFun handler) { SetFatalHandler(std::move(handler)); }
        static void SetTimeZone(TimeZoneMode mode);
        static TimeZoneMode GetTimeZone();
        static void SetDeduplication(bool enabled, int windowSeconds = 10);
        static void FlushDeduplicated();
        static bool shouldLog(LOG_LEVEL level);
        static bool dedupEnabled();
        static int dedupWindowSeconds();
        static logsys::LOG_LEVEL getLogLevel();
        static void SetLogLevel(const LOG_LEVEL &level);
        Logger(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname,
               const int line, LogCategory category = LogCategory::General);
        ~Logger();
        logsys::LogMessage &stream() { return impl_; }
    private:
        logsys::LogMessage impl_;
        LogCategory category_;
        static std::atomic<logsys::LOG_LEVEL> s_level_;
        static bool s_flushOnEachMessage_;
        static std::function<void()> s_fatalHandler_;
        static std::atomic<TimeZoneMode> s_timeZone_;
        static std::atomic<bool> s_dedupEnabled_;
        static std::atomic<int> s_dedupWindowSeconds_;
        static std::mutex s_configMutex_;
    };
} // namespace logsys
#endif // LOGGER_HPP
