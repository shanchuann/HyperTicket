#ifndef INET_TIMESTAMP_HPP
#define INET_TIMESTAMP_HPP

#include <stdint.h>
#include <string>

#include "../../ChronoLite/include/Timestamp.hpp"

namespace shanchuan
{
    class Timestamp
    {
    public:
        Timestamp() = default;
        explicit Timestamp(int64_t micro_sec) : inner_(micro_sec) {}
        explicit Timestamp(const logsys::Timestamp &ts) : inner_(ts) {}

        bool valid() const { return inner_.valid(); }
        int64_t getMicroSec() const { return inner_.getMicroSec(); }
        int64_t getMicro() const { return inner_.getMicroSec(); }
        std::string toString() const { return inner_.toString(); }
        std::string toFormattedString(bool showMic = true) const { return inner_.toFormattedString(showMic); }
        std::string toFileString(bool showMic = true) const { return inner_.toFileString(showMic); }

        static const int64_t kMicroSecPerSecond = logsys::Timestamp::kMicroSecPerSecond;
        static const int64_t kMinPerSec = logsys::Timestamp::kMicroSecPerSecond;

        static Timestamp Now() { return Timestamp(logsys::Timestamp::Now()); }
        static Timestamp invalid() { return Timestamp(logsys::Timestamp::invalid()); }
        static Timestamp Invalid() { return invalid(); }

    private:
        logsys::Timestamp inner_;
    };

    inline Timestamp addTime(const Timestamp &timestamp, double seconds)
    {
        int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecPerSecond);
        return Timestamp(timestamp.getMicroSec() + delta);
    }

    inline Timestamp addMicroTime(const Timestamp &timestamp, int64_t microseconds)
    {
        return Timestamp(timestamp.getMicroSec() + microseconds);
    }

    inline Timestamp addmsTime(const Timestamp &timestamp, size_t milliseconds)
    {
        return addMicroTime(timestamp, static_cast<int64_t>(milliseconds) * 1000);
    }

    inline bool operator<(const Timestamp &lhs, const Timestamp &rhs)
    {
        return lhs.getMicroSec() < rhs.getMicroSec();
    }

    inline bool operator==(const Timestamp &lhs, const Timestamp &rhs)
    {
        return lhs.getMicroSec() == rhs.getMicroSec();
    }
} // namespace shanchuan

#endif // INET_TIMESTAMP_HPP
