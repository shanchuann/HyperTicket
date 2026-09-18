#include "LogCommon.hpp"
#include "Timestamp.hpp"
#include "LogFile.hpp"
#include "Platform.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <vector>

namespace logsys
{
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
        ss << "." << processId() << ".log";
        filename += ss.str();
        return filename;
    }
    LogFile::LogFile(const std::string &basename, size_t rollSize, int flushInterval, int checkEventN,
                     bool threadSafe, std::size_t maxFiles)
        : LogFile(basename, LogFileOptions{rollSize, flushInterval, checkEventN, threadSafe, maxFiles, 0, 0}) {}

    LogFile::LogFile(const std::string &basename, const LogFileOptions &options)
        : basename_(basename),
          rollSize_(std::max<std::size_t>(1, options.rollSize)),
          flushInterval_(std::max(1, options.flushInterval)),
          checkEventN_(std::max(1, options.checkEventN)),
          maxFiles_(options.maxFiles),
          maxTotalBytes_(options.maxTotalBytes),
          maxAgeDays_(options.maxAgeDays),
          count_(0),
          startOfPeriod_(0),
          lastRoll_(0),
          lastFlush_(0),
          file_{nullptr},
          mutex_{options.threadSafe ? new std::mutex{}: nullptr} { rollFile(); }
    LogFile::~LogFile() { }
    void LogFile::append(const std::string &msg) { append(msg.c_str(), msg.size()); }
    void LogFile::append(const char *msg, const size_t len) {
        if(mutex_) {
            std::unique_lock<std::mutex> lock(*mutex_);
            append_unlocked(msg, len);
        }
        else append_unlocked(msg, len); 
    }
    void LogFile::flush() {
        if (mutex_) {
            std::lock_guard<std::mutex> lock(*mutex_);
            file_->flush();
        } else {
            file_->flush();
        }
    }
    bool LogFile::rollFile() {
        logsys::Timestamp now = logsys::Timestamp::Now();
        std::string filename = getLogFileName(basename_, now);
        time_t start = (now.getSeconds() / kRollPerSeconds_) * kRollPerSeconds_;
        if (now.getSeconds() > lastRoll_) {
            lastRoll_ = now.getSeconds();
            lastFlush_ = now.getSeconds();
            startOfPeriod_ = start;
            file_.reset(new logsys::AppendFile(filename));
            removeOldFiles();
            return true;
        }
        return false;
    }

    void LogFile::removeOldFiles() {
        namespace fs = std::filesystem;
        const fs::path path(basename_);
        const fs::path directory = path.has_parent_path() ? path.parent_path() : fs::current_path();
        const std::string prefix = path.filename().string() + ".";
        std::vector<fs::directory_entry> files;
        std::error_code error;
        for (const auto &entry : fs::directory_iterator(directory, error)) {
            if (error) break;
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError) continue;
            const std::string name = entry.path().filename().string();
            if (name.rfind(prefix, 0) == 0 && entry.path().extension() == ".log") files.push_back(entry);
        }
        std::sort(files.begin(), files.end(), [](const auto &left, const auto &right) {
            return left.path().filename().string() > right.path().filename().string();
        });
        const auto now = fs::file_time_type::clock::now();
        for (std::size_t index = files.size(); index > 0; --index) {
            const auto &entry = files[index - 1];
            bool remove = false;
            if (maxAgeDays_ > 0) {
                std::error_code timeError;
                const auto lastWrite = entry.last_write_time(timeError);
                if (!timeError && now - lastWrite > std::chrono::hours(24 * maxAgeDays_)) remove = true;
            }
            if (maxFiles_ > 0 && index > maxFiles_) remove = true;
            if (remove) {
                std::error_code removeError;
                fs::remove(entry.path(), removeError);
            }
        }
        if (maxTotalBytes_ > 0) {
            std::uint64_t total = 0;
            for (const auto &entry : files) {
                std::error_code sizeError;
                if (fs::exists(entry.path(), sizeError) && !sizeError) {
                    const auto size = entry.file_size(sizeError);
                    if (!sizeError) total += size;
                }
            }
            for (std::size_t index = files.size(); index > 0 && total > maxTotalBytes_; --index) {
                const auto &entry = files[index - 1];
                std::error_code sizeError;
                if (!fs::exists(entry.path(), sizeError) || sizeError) continue;
                const auto size = entry.file_size(sizeError);
                if (sizeError) continue;
                std::error_code removeError;
                if (fs::remove(entry.path(), removeError) && !removeError) {
                    total = total > size ? total - size : 0;
                }
            }
        }
    }
}
