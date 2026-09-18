#ifndef LOGMESSAGE_HPP
#define LOGMESSAGE_HPP

#include <string>
#include <sstream>
#include "LogCommon.hpp"
#include "Timestamp.hpp"

namespace logsys
{
    class LogMessage
    {
    private:
        std::string header_;        // 日志头部信息，包含时间戳、日志等级、文件名、函数名和行号等
        std::ostringstream text_;   // 日志正文信息，避免每次插入都创建临时流
        logsys::LOG_LEVEL level_;   // 日志等级
        std::string filename_;
        std::string funcname_;
        int line_;
    public:
        LogMessage(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname,
                   const int line, TimeZoneMode mode = TimeZoneMode::Local);
        ~LogMessage();
        const logsys::LOG_LEVEL &getLogLevel() const;
        void setLogLevel(const logsys::LOG_LEVEL &level);
        const std::string toString() const;
        const std::string body() const { return text_.str(); }
        std::string sourceKey() const;
        template <typename T>
        LogMessage &operator<<(const T &text) {
            text_ << ": " << text;
            return *this;
        }
    };
} // namespace logsys
#endif // LOGMESSAGE_HPP
