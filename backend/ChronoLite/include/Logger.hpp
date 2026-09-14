#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "LogMessage.hpp"
#include <atomic>
#include <string>
#include <functional>

#define LOG_TRACE    \
    if (logsys::Logger::getLogLevel() <= logsys::LOG_LEVEL::TRACE) \
    logsys::Logger(logsys::LOG_LEVEL::TRACE, __FILE__, __func__, __LINE__).stream()
#define LOG_DEBUG    \
    if (logsys::Logger::getLogLevel() <= logsys::LOG_LEVEL::DEBUG) \
    logsys::Logger(logsys::LOG_LEVEL::DEBUG, __FILE__, __func__, __LINE__).stream()
#define LOG_INFO     \
    if (logsys::Logger::getLogLevel() <= logsys::LOG_LEVEL::INFO ) \
    logsys::Logger(logsys::LOG_LEVEL::INFO , __FILE__, __func__, __LINE__).stream()
#define LOG_WARN     \
    logsys::Logger(logsys::LOG_LEVEL::WARN , __FILE__, __func__, __LINE__).stream()
#define LOG_ERROR    \
    logsys::Logger(logsys::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__).stream()
#define LOG_FATAL    \
    logsys::Logger(logsys::LOG_LEVEL::FATAL, __FILE__, __func__, __LINE__).stream()
#define LOG_SYSERR   \
    logsys::Logger(logsys::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__).stream()
#define LOG_SYSFATAL \
    logsys::Logger(logsys::LOG_LEVEL::FATAL, __FILE__, __func__, __LINE__).stream()

namespace logsys
{
    class Logger
    {
    public:
        using OutputFun = std::function<void(const std::string &)>;
        using FlushFun  = std::function<void(void)>;
        using FatalFun  = std::function<void(void)>; // FATAL 前的清理回调：应用层注册（如停止异步日志线程），不得再写日志

        static OutputFun s_output_;
        static FlushFun  s_flush_;
        static void SetOuput(OutputFun fun);
        static void SetFlush(FlushFun  fun);
        static void SetFatal(FatalFun  fun);  // 注册 fatal 清理回调

        static logsys::LOG_LEVEL getLogLevel();
        static void SetLogLevel(const LOG_LEVEL &level);

        Logger(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname, const int line);
        ~Logger();
        logsys::LogMessage &stream() { return impl_; }

    private:
        logsys::LogMessage impl_;
        static std::atomic<LOG_LEVEL> s_level_;
        static FatalFun s_fatal_;  // 默认为空，应用层可注册
    };
} // namespace logsys
#endif // LOGGER_HPP