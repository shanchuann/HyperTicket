#ifndef LOGFILE_HPP
#define LOGFILE_HPP

#include <string>
#include <memory>
#include <mutex>
#include <time.h>
#include <cstddef>
#include <cstdint>
#include "Timestamp.hpp"
#include "AppendFile.hpp"

namespace logsys
{
    struct LogFileOptions {
        std::size_t rollSize = 1024 * 128;
        int flushInterval = 3;
        int checkEventN = 30;
        bool threadSafe = true;
        std::size_t maxFiles = 0;
        std::uint64_t maxTotalBytes = 0;
        int maxAgeDays = 0;
    };

    class LogFile
    {
    private:
        const std::string basename_;
        const size_t rollSize_;
        const int flushInterval_;
        const int checkEventN_;
        const std::size_t maxFiles_;
        const std::uint64_t maxTotalBytes_;
        const int maxAgeDays_;
        int count_;
        time_t startOfPeriod_;
        time_t lastRoll_;
        time_t lastFlush_;
        static const int kRollPerSeconds_ = 60 * 60 * 24;
        std::unique_ptr<logsys::AppendFile> file_;
        std::unique_ptr<std::mutex> mutex_;
        void append_unlocked(const char *msg, const size_t len);
        void removeOldFiles();
        static std::string getLogFileName(const std::string &basename,const logsys::Timestamp &now);
    public:
        LogFile(const std::string &basename, size_t rollSize = 1024 * 128, int flushInterval = 3,
                int checkEventN = 30, bool threadSafe = true, std::size_t maxFiles = 0);
        LogFile(const std::string &basename, const LogFileOptions &options);
        ~LogFile();
        void append(const std::string &msg);
        void append(const char *msg, const size_t len);
        void flush();
        bool rollFile();
    };
} // namespace logsys
#endif // LOGFILE_HPP
