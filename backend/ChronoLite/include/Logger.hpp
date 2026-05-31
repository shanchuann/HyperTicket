#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "LogMessage.hpp"
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
        static OutputFun s_output_;
        static FlushFun  s_flush_;
        static void SetOuput(OutputFun fun);
        static void SetFlush(FlushFun  fun);
        static logsys::LOG_LEVEL getLogLevel();
        static void SetLogLevel(const LOG_LEVEL &level);
        Logger(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname, const int line);
        ~Logger();
        logsys::LogMessage &stream() { return impl_; }
    private:
        logsys::LogMessage impl_;
        static logsys::LOG_LEVEL s_level_;
    };
} // namespace logsys
#endif // LOGGER_HPP