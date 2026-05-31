#ifndef LOGMESSAGE_HPP
#define LOGMESSAGE_HPP

#include <string>
#include <sstream>
#include "LogCommon.hpp"

namespace logsys
{
    class LogMessage
    {
    private:
        std::string header_;        // 日志头部信息，包含时间戳、日志等级、文件名、函数名和行号等
        std::string text_;          // 日志正文信息，包含用户通过 operator<< 追加的日志内容
        logsys::LOG_LEVEL level_;   // 日志等级
    public:
        LogMessage(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname, const int line);
        ~LogMessage();
        const logsys::LOG_LEVEL &getLogLevel() const;
        void setLogLevel(const logsys::LOG_LEVEL &level);
        const std::string toString() const;
        template <typename T>
        LogMessage &operator<<(const T &text) {
            std::stringstream sout;
            sout << ": " << text;
            text_ += sout.str();
            return *this;
        }
    };
} // namespace logsys
#endif // LOGMESSAGE_HPP