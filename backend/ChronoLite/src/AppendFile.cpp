#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "AppendFile.hpp"

namespace logsys
{
    size_t AppendFile::write(const char *msg, const size_t len) {
        return std::fwrite(msg, sizeof(char), len, fp_);
    }
    AppendFile::AppendFile(const std::string &filename) : buffer_{new char[FILE_BUFF_SIZE]}, fp_{nullptr}, writenBytes_{0}, failedWrites_{0} {
        fp_ = std::fopen(filename.c_str(), "ab");
        if (!fp_) {
            throw std::runtime_error("failed to open log file '" + filename + "': " + std::strerror(errno));
        }
        std::setvbuf(fp_, buffer_.get(), _IOFBF, FILE_BUFF_SIZE);
    }
    AppendFile::~AppendFile() {
        if (fp_) std::fclose(fp_);
        fp_ = nullptr;
        buffer_.reset();
    }
    void AppendFile::append(const std::string &msg) { append(msg.c_str(), msg.size()); }
    void AppendFile::append(const char *msg, const size_t len) {
        if (len == 0) return;
        size_t n = write(msg, len);
        size_t remain = len - n;
        while (remain > 0) {
            size_t x = write(msg + n, remain);
            if (x == 0) {
                int err = ferror(fp_);
                if (err) {
                    ++failedWrites_;
                    std::fprintf(stderr, "AppendFile::append() failed: %s\n", std::strerror(errno));
                    break;
                }
                ++failedWrites_;
                std::fprintf(stderr, "AppendFile::append() failed: short write\n");
                break;
            }
            n += x;
            remain = len - n;
        }
        writenBytes_ += n;
    }
    void AppendFile::flush() { fflush(fp_); }
    size_t AppendFile::getWriteBytes() const { return writenBytes_; }
    size_t AppendFile::getFailedWrites() const { return failedWrites_; }
}
