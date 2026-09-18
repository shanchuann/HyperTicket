#ifndef LOGCOMMON_HPP
#define LOGCOMMON_HPP

namespace logsys
{
    static const int SMALL_BUFF_LEN  = 128;  // 小缓冲区大小，适用于日志头部等较短的字符串
    static const int MEDIAN_BUFF_LEN = 512;  // 中等缓冲区大小. 适用于一般日志消息
    static const int LARGE_BUFF_LEN  = 1024; // 大缓冲区大小,适用于长日志消息或包含大量上下文信息的日志
    enum class LOG_LEVEL {
        FATAL = 0,          // 严重错误信息
        ERROR,              // 错误信息
        WARN,               // 警告信息
        INFO,               // 日志信息，通常用于记录程序的关键事件、状态变化和重要操作
        DEBUG,              // 调试信息，通常用于调试和开发
        TRACE,              // 跟踪信息
        NUM_LOG_LEVELS,     // 日志等级数量
    };
    inline constexpr const char *LLTOSTR[] = {
        "FATAL",    // 0
        "ERROR",    // 1
        "WARN ",    // 2
        "INFO ",    // 3
        "DEBUG",    // 4
        "TRACE",    // 5
        "NUM_LOG_LEVELS"
    };
    enum class LogCategory { General, Audit, UserAction, Transaction, TraceableError };

    constexpr bool shouldLog(LOG_LEVEL message, LOG_LEVEL threshold) {
        return static_cast<int>(message) <= static_cast<int>(threshold);
    }
} // namespace logsys
#endif // LOGCOMMON_HPP
