#ifndef LOGCOMMON_HPP
#define LOGCOMMON_HPP

namespace logsys
{
    static const int SMALL_BUFF_LEN  = 128;  // 小缓冲区大小，适用于日志头部等较短的字符串
    static const int MEDIAN_BUFF_LEN = 512;  // 中等缓冲区大小. 适用于一般日志消息
    static const int LARGE_BUFF_LEN  = 1024; // 大缓冲区大小,适用于长日志消息或包含大量上下文信息的日志
    enum class LOG_LEVEL {
        TRACE = 0,          // 跟踪信息
        DEBUG,              // 调试信息，通常用于调试和开发
        INFO,               // 日志信息，通常用于记录程序的关键事件、状态变化和重要操作
        WARN,               // 警告信息
        ERROR,              // 错误信息
        FATAL,              // 严重错误信息
        NUM_LOG_LEVELS,     // 日志等级数量
    };
    static const char *LLTOSTR[] = {
        "TRACE",    // 0
        "DEBUG",    // 1
        "INFO ",    // 2
        "WARN ",    // 3
        "ERROR",    // 4
        "FATAL",    // 5
        "NUM_LOG_LEVELS"
    };
} // namespace logsys
#endif // LOGCOMMON_HPP