#ifndef HYPERTICKET_SERVICE_UTIL_HPP
#define HYPERTICKET_SERVICE_UTIL_HPP

#include <cstdint>
#include <sstream>
#include <string>

#include "../../ChronoLite/include/Timestamp.hpp"

namespace hyperticket
{
    // 当前时间（毫秒），用于会话 TTL。
    inline int64_t nowMs()
    {
        return logsys::Timestamp::Now().getMicroSec() / 1000;
    }

    // 与原 ser.cpp 一致的口令哈希（FNV-1a 64-bit，确定性）。注意：仅为示例强度，
    // 生产应替换为 bcrypt/argon2。集中于此一处，避免散落。
    inline std::string hashPassword(const std::string &password)
    {
        uint64_t hash = 1469598103934665603ULL;
        for (unsigned char ch : password)
        {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        std::ostringstream oss;
        oss << std::hex << hash;
        return oss.str();
    }
} // namespace hyperticket
#endif // HYPERTICKET_SERVICE_UTIL_HPP
