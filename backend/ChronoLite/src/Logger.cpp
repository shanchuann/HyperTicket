#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <utility>
#include "Logger.hpp"

namespace logsys
{
    namespace {
        struct DedupEntry {
            std::string lastMessage;
            std::chrono::steady_clock::time_point lastSeen;
            std::size_t repeats = 0;
        };
        std::mutex dedupMutex;
        std::unordered_map<std::string, DedupEntry> dedupEntries;

        bool dedupAllowed(LogCategory category, LOG_LEVEL level) {
            return category == LogCategory::General && level != LOG_LEVEL::FATAL;
        }

        std::string dedupKey(LOG_LEVEL level, const std::string &source, const std::string &body) {
            return std::to_string(static_cast<int>(level)) + "|" + source + "|" + body;
        }

        std::vector<std::string> deduplicate(LOG_LEVEL level, LogCategory category,
                                              const std::string &source, const std::string &body,
                                              const std::string &message) {
            if (!Logger::dedupEnabled() || !dedupAllowed(category, level)) {
                return {message};
            }
            const auto now = std::chrono::steady_clock::now();
            const auto window = std::chrono::seconds(Logger::dedupWindowSeconds());
            const std::string key = dedupKey(level, source, body);
            std::lock_guard<std::mutex> lock(dedupMutex);
            auto &entry = dedupEntries[key];
            if (entry.lastMessage.empty() || now - entry.lastSeen > window) {
                std::vector<std::string> result;
                if (!entry.lastMessage.empty() && entry.repeats > 0) {
                    result.push_back(entry.lastMessage + " [repeated " + std::to_string(entry.repeats) + " times]");
                }
                entry.lastMessage = message;
                entry.lastSeen = now;
                entry.repeats = 0;
                if (dedupEntries.size() > 4096) dedupEntries.erase(dedupEntries.begin());
                result.push_back(message);
                return result;
            }
            entry.lastSeen = now;
            entry.repeats += 1;
            return {};
        }
    }

    void defaultOutput(const std::string &msg) { 
        std::fwrite(msg.c_str(), sizeof(char), msg.size(), stdout);
    }
    void defaultFlush() { 
        std::fflush(stdout);
    }
    logsys::LOG_LEVEL InitLogLevel() {
        if (::getenv("LOGSYS::LOG_TRACE")) return logsys::LOG_LEVEL::TRACE;
        else if (::getenv("LOGSYS::LOG_DEBUG")) return logsys::LOG_LEVEL::DEBUG;
        else return logsys::LOG_LEVEL::INFO;
    }
    Logger::OutputFun Logger::s_output_ = defaultOutput;
    Logger::FlushFun  Logger::s_flush_  = defaultFlush;
    bool Logger::s_flushOnEachMessage_ = true;
    std::function<void()> Logger::s_fatalHandler_ = [] { std::exit(EXIT_FAILURE); };
    std::atomic<TimeZoneMode> Logger::s_timeZone_{TimeZoneMode::Local};
    std::atomic<bool> Logger::s_dedupEnabled_{false};
    std::atomic<int> Logger::s_dedupWindowSeconds_{10};
    std::mutex Logger::s_configMutex_;
    void Logger::SetOutput(OutputFun out) {
        std::lock_guard<std::mutex> lock(s_configMutex_);
        s_output_ = std::move(out);
    }
    void Logger::SetOuput(OutputFun out)  { SetOutput(std::move(out)); }
    void Logger::SetFlush(FlushFun flush) {
        std::lock_guard<std::mutex> lock(s_configMutex_);
        s_flush_ = std::move(flush);
    }
    void Logger::SetFlushOnEachMessage(bool enabled) {
        std::lock_guard<std::mutex> lock(s_configMutex_);
        s_flushOnEachMessage_ = enabled;
    }
    void Logger::SetFatalHandler(std::function<void()> handler) {
        std::lock_guard<std::mutex> lock(s_configMutex_);
        s_fatalHandler_ = handler ? std::move(handler) : [] { std::exit(EXIT_FAILURE); };
    }
    void Logger::SetTimeZone(TimeZoneMode mode) { s_timeZone_.store(mode, std::memory_order_relaxed); }
    TimeZoneMode Logger::GetTimeZone() { return s_timeZone_.load(std::memory_order_relaxed); }
    void Logger::SetDeduplication(bool enabled, int windowSeconds) {
        const int normalizedWindow = std::max(1, windowSeconds);
        const int previousWindow = s_dedupWindowSeconds_.exchange(normalizedWindow, std::memory_order_relaxed);
        if (!enabled || previousWindow != normalizedWindow) {
            std::lock_guard<std::mutex> lock(dedupMutex);
            dedupEntries.clear();
        }
        s_dedupEnabled_.store(enabled, std::memory_order_relaxed);
    }
    bool Logger::shouldLog(LOG_LEVEL level) { return logsys::shouldLog(level, getLogLevel()); }
    bool Logger::dedupEnabled() { return s_dedupEnabled_.load(std::memory_order_relaxed); }
    int Logger::dedupWindowSeconds() { return s_dedupWindowSeconds_.load(std::memory_order_relaxed); }
    void Logger::FlushDeduplicated() {
        OutputFun output;
        FlushFun flush;
        {
            std::lock_guard<std::mutex> lock(s_configMutex_);
            output = s_output_;
            flush = s_flush_;
        }
        std::vector<std::string> summaries;
        {
            std::lock_guard<std::mutex> lock(dedupMutex);
            for (auto &[key, entry] : dedupEntries) {
                if (entry.repeats > 0) {
                    summaries.push_back(entry.lastMessage + " [repeated " + std::to_string(entry.repeats) + " times]\n");
                    entry.repeats = 0;
                }
            }
        }
        for (const auto &summary : summaries) output(summary);
        if (!summaries.empty()) flush();
    }
    Logger::Logger(const logsys::LOG_LEVEL &level, const std::string &filename, const std::string &funcname,
                   const int line, LogCategory category)
        : impl_(level, filename, funcname, line, GetTimeZone()), category_(category) {}
    Logger::~Logger() {
        impl_ << "\n";
        OutputFun output;
        FlushFun flush;
        std::function<void()> fatalHandler;
        bool flushEachMessage;
        {
            std::lock_guard<std::mutex> lock(s_configMutex_);
            output = s_output_;
            flush = s_flush_;
            fatalHandler = s_fatalHandler_;
            flushEachMessage = s_flushOnEachMessage_;
        }
        for (const auto &message : deduplicate(impl_.getLogLevel(), category_, impl_.sourceKey(), impl_.body(), impl_.toString())) {
            output(message);
        }
        if (flushEachMessage) flush();
        if (impl_.getLogLevel() == LOG_LEVEL::FATAL) {
            std::fprintf(stderr, "PROCESS EXIT\n");
            fatalHandler();
        }
    }
    std::atomic<logsys::LOG_LEVEL> Logger::s_level_{InitLogLevel()};
    logsys::LOG_LEVEL Logger::getLogLevel() { return s_level_.load(std::memory_order_relaxed); }
    void Logger::SetLogLevel(const LOG_LEVEL &level) { s_level_.store(level, std::memory_order_relaxed); }
}
