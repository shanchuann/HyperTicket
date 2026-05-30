#ifndef APPENDFILE_HPP
#define APPENDFILE_HPP

#include <stdio.h>
#include <string>
#include <memory>

namespace logsys
{
    class AppendFile
    {
    private:
        static const size_t FILE_BUFF_SIZE = 128 * 1024; // 128k 测试时采用小缓冲区，应用后应调整为1M或更大
        std::unique_ptr<char[]> buffer_; 
        FILE *fp_;
        size_t writenBytes_;
        size_t write(const char *msg, const size_t len);
    public:
        AppendFile(const std::string &filename);
        ~AppendFile();
        void append(const std::string &msg);             // C++ 风格
        void append(const char *msg, const size_t len);  // C 风格
        void flush();
        size_t getWriteBytes() const;
    };
} // namespace logsys
#endif // APPENDFILE_HPP