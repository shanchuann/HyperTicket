#ifndef HYPERTICKET_SERVICE_UTIL_HPP
#define HYPERTICKET_SERVICE_UTIL_HPP

#include <cstdint>
#include <sstream>
#include <string>
#include <random>

#include "../../ChronoLite/include/Timestamp.hpp"

namespace hyperticket
{
    // 当前时间（毫秒），用于会话 TTL。
    inline int64_t nowMs()
    {
        return logsys::Timestamp::Now().getMicroSec() / 1000;
    }

    // ========== 密码哈希（bcrypt，通过 POSIX crypt） ==========

    // 恒定时间字符串比较：始终扫描全部字节，防止时序攻击泄露哈希前缀。
    // 即使长度不同也完整比较（先 XOR 长度差异到结果中）。
    inline bool constantTimeEqual(const std::string &a, const std::string &b)
    {
        const size_t lenA = a.size(), lenB = b.size();
        volatile unsigned char diff = static_cast<unsigned char>(lenA ^ lenB);
        const size_t minLen = lenA < lenB ? lenA : lenB;
        for (size_t i = 0; i < minLen; ++i)
            diff |= static_cast<unsigned char>(a[i] ^ b[i]);
        // 长度不同时额外扫描较长串的剩余部分，保证时间与较长串成比例
        for (size_t i = minLen; i < (lenA > lenB ? lenA : lenB); ++i)
            diff |= static_cast<unsigned char>(
                (i < lenA ? a[i] : 0) ^ (i < lenB ? b[i] : 0));
        return diff == 0;
    }

    // 生成 bcrypt salt（$2b$12$...）。
    inline std::string generateBcryptSalt()
    {
        // 22 位 base64 随机盐，bcrypt 要求
        static const char b64[] =
            "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 63);
        std::string salt(22, '.');
        for (int i = 0; i < 22; ++i)
            salt[i] = b64[dist(gen)];
        return "$2b$12$" + salt;
    }

    // 用 bcrypt 哈希密码。返回格式 "$2b$12$..."（60 字符）。
    std::string hashPassword(const std::string &password);

    // 验证密码。自动识别 bcrypt（$2b$开头）和旧 FNV-1a 哈希。
    // 返回 true 表示密码正确；needRehash 表示该用户需要升级到 bcrypt。
    bool verifyPassword(const std::string &password, const std::string &storedHash,
                        bool &needRehash);

    // ========== 旧 FNV-1a 哈希（仅用于向后兼容验证） ==========

    inline std::string fnv1aHash(const std::string &password)
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
