#include <stdio.h>
#include "Logger.hpp"

namespace logsys
{
    void defaultOutput(const std::string &msg) { 
        size_t n = fwrite(msg.c_str(), sizeof(char), msg.size(), stdout); 
    }
    void defaultFlush() { 
        fflush(stdout); 
    }
    logsys::LOG_LEVEL InitLogLevel() {
        if (::getenv("LOGSYS::LOG_TRACE")) return logsys::LOG_LEVEL::TRACE;
        else if (::getenv("LOGSYS::LOG_DEBUG")) return logsys::LOG_LEVEL::DEBUG;
        else return logsys::LOG_LEVEL::INFO;
    }
    Logger::OutputFun Logger::s_output_ = defaultOutput;
    Logger::FlushFun  Logger::s_flush_  = defaultFlush;
    void Logger::SetOuput(OutputFun out)  { s_output_ = out;  }
    void Logger::SetFlush(FlushFun flush) { s_flush_ = flush; }
    Logger::Logger(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname, const int line)
        : impl_(level, filename, funcname, line) {}
    Logger::~Logger() {
        impl_ << "\n";
        s_output_(impl_.toString());
        s_flush_();
        if (impl_.getLogLevel() == LOG_LEVEL::FATAL) {
            fprintf(stderr, "PROCESS EXIT \n");
            exit(EXIT_FAILURE);
        }
    }
    logsys::LOG_LEVEL Logger::s_level_ = InitLogLevel();
    logsys::LOG_LEVEL Logger::getLogLevel() { return s_level_; }
    void Logger::SetLogLevel(const LOG_LEVEL &level) { s_level_ = level; }
}