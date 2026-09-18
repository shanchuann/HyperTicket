#include "Timestamp.hpp"
#include "LogCommon.hpp"
#include <cstdio>
#include <cinttypes>
#include <time.h>
#include <utility>
#ifdef _WIN32
#include <chrono>
// Windows 下没有 localtime_r，使用 localtime_s 替代，参数顺序相反
#define localtime_r(timep, result) localtime_s(result, timep)
#else
#include <sys/time.h>
#endif

namespace logsys {
    const int Buffsize = SMALL_BUFF_LEN; // 格式化字符串缓冲区大小

    Timestamp::Timestamp() : microSec(0) {} // 创建一个无效的时间戳（microSec = 0）
    Timestamp::Timestamp(const int64_t ms) : microSec(ms) {}
    Timestamp::~Timestamp() {}
    void Timestamp::swap(Timestamp& other) {
        std::swap(microSec, other.microSec);
    }
    bool Timestamp::valid() const {
        return microSec > 0;
    }
    std::string Timestamp::toString(TimeZoneMode mode) const { // 秒.微秒
        char buff[Buffsize] = {0};
        time_t  second = microSec / kMicroSecPerSecond;
        int64_t micsec = microSec % kMicroSecPerSecond;
        std::snprintf(buff, sizeof(buff), "%lld.%06lld%s",
                      static_cast<long long>(second), static_cast<long long>(micsec),
                      mode == TimeZoneMode::UTC ? "Z" : "");
        return std::string(buff);
    }
    std::string Timestamp::toFormattedString(bool showMic, TimeZoneMode mode) const { // YYYY/MM/DD HH:MM:SS[.微秒]
        char buff[Buffsize] = {0};
        time_t  second = microSec / kMicroSecPerSecond;
        int64_t micsec = microSec % kMicroSecPerSecond;
        struct tm time = {};
        // 根据配置选择本地时间或 UTC。
 #ifdef _WIN32
        const int timeResult = mode == TimeZoneMode::UTC ? gmtime_s(&time, &second) : localtime_s(&time, &second);
        if (timeResult != 0) return {};
 #else
        const auto *timeResult = mode == TimeZoneMode::UTC ? gmtime_r(&second, &time) : localtime_r(&second, &time);
        if (timeResult == nullptr) return {};
 #endif
        int pos = std::snprintf(buff, sizeof(buff), "%04d/%02d/%02d %02d:%02d:%02d",
                                time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
                                time.tm_hour, time.tm_min, time.tm_sec);
        if(showMic) pos += std::snprintf(buff + pos, sizeof(buff) - static_cast<size_t>(pos), ".%06lld", static_cast<long long>(micsec));
        if (mode == TimeZoneMode::UTC) std::snprintf(buff + pos, sizeof(buff) - static_cast<size_t>(pos), "Z");
        return std::string(buff);
    }
    std::string Timestamp::toFileString(bool showMic, TimeZoneMode mode) const { // YYYYMMDD-HHMMSS[.微秒]
        char buff[Buffsize] = {0};
        time_t  second = microSec / kMicroSecPerSecond;
        int64_t micsec = microSec % kMicroSecPerSecond;
        struct tm time = {};
 #ifdef _WIN32
        const int timeResult = mode == TimeZoneMode::UTC ? gmtime_s(&time, &second) : localtime_s(&time, &second);
        if (timeResult != 0) return {};
 #else
        const auto *timeResult = mode == TimeZoneMode::UTC ? gmtime_r(&second, &time) : localtime_r(&second, &time);
        if (timeResult == nullptr) return {};
 #endif
        int pos = std::snprintf(buff, sizeof(buff), "%04d%02d%02d-%02d%02d%02d",
                                time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
                                time.tm_hour, time.tm_min, time.tm_sec);
        if(showMic) pos += std::snprintf(buff + pos, sizeof(buff) - static_cast<size_t>(pos), ".%06lld", static_cast<long long>(micsec));
        if (mode == TimeZoneMode::UTC) std::snprintf(buff + pos, sizeof(buff) - static_cast<size_t>(pos), "Z");
        return std::string(buff);
    }
    int64_t Timestamp::getMicroSec() const {
        return microSec;
    }
    time_t Timestamp::getSeconds() const {
        return static_cast<time_t>(microSec / kMicroSecPerSecond);
    }
    Timestamp Timestamp::Now() { // 获取当前时间戳
#ifdef _WIN32
        // Windows 使用 std::chrono 获取高精度时间
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
        return Timestamp(duration.count());
#else
        // Linux 使用 gettimeofday 获取时间
        struct timeval time;
        gettimeofday(&time, NULL); // NULL 表示使用系统时区信息
        // 计算从 1970年1月1日 00:00:00 到现在的微秒数
        int64_t micro = time.tv_sec*kMicroSecPerSecond + time.tv_usec;
        return Timestamp(micro);
#endif
    }
    Timestamp Timestamp::invalid() { // 获取一个无效的时间戳
        return Timestamp();
    }
}
