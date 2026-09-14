#include <stdio.h>
#include <cstdlib>
#include "Logger.hpp"

namespace logsys
{
    void defaultOutput(const std::string &msg) { 
        size_t n = fwrite(msg.c_str(), sizeof(char), msg.size(), stdout); 
    }
    void defaultFlush() { 
        fflush(stdout); 
    }
    /*
    环境变量控制日志等级，优先级从高到低依次为 FATAL > ERROR > WARN > INFO > DEBUG > TRACE，如果没有设置任何环境变量，则默认日志等级为 INFO。
    当前项目使用config.json配置文件来控制日志等级，因此注释掉环境变量控制日志等级的代码。
    logsys::LOG_LEVEL InitLogLevel() {
        if      (::getenv("LOGSYS_LOG_TRACE")) return logsys::LOG_LEVEL::TRACE;
        else if (::getenv("LOGSYS_LOG_DEBUG")) return logsys::LOG_LEVEL::DEBUG;
        else if (::getenv("LOGSYS_LOG_INFO"))  return logsys::LOG_LEVEL:: INFO;
        else if (::getenv("LOGSYS_LOG_WARN"))  return logsys::LOG_LEVEL:: WARN;
        else if (::getenv("LOGSYS_LOG_ERROR")) return logsys::LOG_LEVEL::ERROR;
        else if (::getenv("LOGSYS_LOG_FATAL")) return logsys::LOG_LEVEL::FATAL;
        else return logsys::LOG_LEVEL::INFO; // 默认日志等级为 INFO
    }
    */

    Logger::OutputFun Logger::s_output_ = defaultOutput;
    Logger::FlushFun  Logger::s_flush_  = defaultFlush;
    Logger::FatalFun  Logger::s_fatal_  = nullptr;        // 默认无清理操作

    void Logger::SetOuput(OutputFun out)  { s_output_ = out;  }
    void Logger::SetFlush(FlushFun flush) { s_flush_  = flush; }
    void Logger::SetFatal(FatalFun  fun)  { s_fatal_  = fun;  }

    Logger::Logger(const logsys::LOG_LEVEL &level, const std::string &filename,
                   const std::string &funcname, const int line)
        : impl_(level, filename, funcname, line) {}

    Logger::~Logger() {
        impl_ << "\n";
        s_output_(impl_.toString());
        s_flush_();   // AsynLogging::flush() 是同步写盘，FATAL 消息此时已落盘

        if (impl_.getLogLevel() == LOG_LEVEL::FATAL) {
            fprintf(stderr, "FATAL:PROCESS EXIT\n");
            // 执行应用层注册的清理（如 asyncLog.stop()），确保后台线程退出
            // 注意：回调内不能再写日志，否则死递归
            if (s_fatal_) s_fatal_();
            // abort() 而非 exit()：
            // 1. 不触发 atexit/静态析构，避免析构过程中再次进入 Logger
            // 2. 发送 SIGABRT，产生 core dump，保留现场供事后调试
            ::abort();
        }
    }
    std::atomic<LOG_LEVEL> Logger::s_level_{LOG_LEVEL::INFO};
    logsys::LOG_LEVEL Logger::getLogLevel() {
        return s_level_.load(std::memory_order_relaxed);
    }
    void Logger::SetLogLevel(const LOG_LEVEL &level) {
        s_level_.store(level, std::memory_order_relaxed);
    }
}