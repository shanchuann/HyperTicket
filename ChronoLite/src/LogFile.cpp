#include <sys/types.h>
#include <unistd.h>
#include "LogCommon.hpp"
#include "Timestamp.hpp"
#include "LogFile.hpp"
#include <string>
#include <sstream>

namespace logsys
{
    const std::string hostname() {
        char buff[SMALL_BUFF_LEN] = {0};
        if (!::gethostname(buff, SMALL_BUFF_LEN)) return std::string(buff);
        else return std::string("unknownhost");
    }
    pid_t pid() { return ::getpid(); }
    void LogFile::append_unlocked(const char *msg, const size_t len) {
        file_->append(msg, len);
        if (file_->getWriteBytes() > rollSize_) rollFile();
        else {
            count_ += 1;
            if (count_ > checkEventN_) {
                count_ = 0;
                time_t now = ::time(nullptr);
                time_t thisPeriod = (now / kRollPerSeconds_) * kRollPerSeconds_;
                if (thisPeriod != startOfPeriod_) rollFile();
                else if (now - lastFlush_ > flushInterval_) {
                    lastFlush_ = now;
                    file_->flush();
                }
            }
        }
    }
    std::string LogFile::getLogFileName(const std::string &basename, const logsys::Timestamp &now) {
        std::string filename;
        filename.reserve(basename.size() + SMALL_BUFF_LEN);
        filename = basename;
        filename += ".";
        filename += now.toFileString(); // YYYYMMDD-HHMMSS[.微秒]
        filename += ".";
        filename += hostname();
        std::stringstream ss;
        ss << "." << pid() << ".log";
        filename += ss.str();
        return filename;
    }
    LogFile::LogFile(const std::string &basename, size_t rollSize, int flushInterval, int checkEventN, bool threadSafe)
        : basename_(basename),
          rollSize_(rollSize),
          flushInterval_(flushInterval),
          checkEventN_(checkEventN),
          mutex_{threadSafe? new std::mutex{}: nullptr},
          count_(0),
          startOfPeriod_(0),
          lastRoll_(0),
          lastFlush_(0) { rollFile(); }
    LogFile::~LogFile() { }
    void LogFile::append(const std::string &msg) { append(msg.c_str(), msg.size()); }
    void LogFile::append(const char *msg, const size_t len) {
        if(mutex_) {
            std::unique_lock<std::mutex> lock(*mutex_);
            append_unlocked(msg, len);
        }
        else append_unlocked(msg, len); 
    }
    void LogFile::flush() { file_->flush(); }
    bool LogFile::rollFile() {
        logsys::Timestamp now = logsys::Timestamp::Now();
        std::string filename = getLogFileName(basename_, now);
        time_t start = (now.getSeconds() / kRollPerSeconds_) * kRollPerSeconds_;
        if (now.getSeconds() > lastRoll_) {
            lastRoll_ = now.getSeconds();
            lastFlush_ = now.getSeconds();
            startOfPeriod_ = start;
            file_.reset(new logsys::AppendFile(filename));
            return true;
        }
        return false;
    }
}