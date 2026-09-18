#include <thread>
#include "LogMessage.hpp"
#include "Timestamp.hpp"

namespace logsys
{
    LogMessage::LogMessage(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname,
                           const int line, TimeZoneMode mode)
        : header_{}, text_{}, level_(level), filename_(filename), funcname_(funcname), line_(line) {
        std::stringstream sout;
        sout << logsys::Timestamp::Now().toFormattedString(false, mode) << " ";
        sout << std::this_thread::get_id() << " "; // 输出线程ID
        sout << logsys::LLTOSTR[static_cast<int>(level_)] << " ";
#if defined(_WIN32) || defined(_WIN64)
        const size_t pos = filename.find_last_of('\\'); // Windows 下路径分隔符是反斜杠
#else
        const size_t pos = filename.find_last_of('/'); // 找到最后一个路径分隔符的位置，提取文件名
#endif
        const std::string fname = filename.substr(pos + 1);
        sout << fname << " " << funcname << " " << line;
        header_ = sout.str();
    }
    LogMessage::~LogMessage() {}
    const logsys::LOG_LEVEL &LogMessage::getLogLevel() const { return level_; }
    void LogMessage::setLogLevel(const logsys::LOG_LEVEL &level) { level_ = level; }
    const std::string LogMessage::toString() const { return header_ + text_.str(); }
    std::string LogMessage::sourceKey() const {
        return filename_ + ":" + funcname_ + ":" + std::to_string(line_);
    }
}
