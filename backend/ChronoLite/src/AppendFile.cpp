#include <cassert>
#include <string.h>
#include <error.h>
#include <mutex>
#include "AppendFile.hpp"

namespace logsys
{
    size_t AppendFile::write(const char *msg, const size_t len) {
#ifdef __linux__
        return fwrite_unlocked(msg, sizeof(char), len, fp_);    // Linux 下的线程安全版本
#else
        std::lock_guard<std::mutex> lock(mutex_);
        return fwrite(msg, sizeof(char), len, fp_);             // 其他平台使用标准的 fwrite, 需要加锁保护
#endif
    }
    AppendFile::AppendFile(const std::string &filename) : fp_{nullptr}, buffer_{new char[FILE_BUFF_SIZE]}, writenBytes_{0} {
        fp_ = fopen(filename.c_str(), "a");                     // 以追加模式打开文件 w为写入模式 a为追加模式
        assert(fp_ != nullptr);
        setbuffer(fp_, buffer_.get(), FILE_BUFF_SIZE);
    }
    AppendFile::~AppendFile() {
        fclose(fp_);
        fp_ = nullptr;
        buffer_.reset();
    }
    void AppendFile::append(const std::string &msg) { append(msg.c_str(), msg.size()); }
    void AppendFile::append(const char *msg, const size_t len) {
        size_t n = write(msg, len); // len 1000
        size_t remain = len - n;
        while (remain > 0) {
            size_t x = write(msg + n, remain);
            if (x == 0) {
                int err = ferror(fp_);
                if (err) {
                    fprintf(stderr, "appendFile::append() failed %s \n", strerror(err));
                    break;
                }
            }
            n += x;
            remain = len - n;
        }
        writenBytes_ += n;
    }
    void AppendFile::flush() { fflush(fp_); }
    size_t AppendFile::getWriteBytes() const { return writenBytes_; }
}