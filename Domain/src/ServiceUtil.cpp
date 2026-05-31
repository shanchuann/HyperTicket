#include "../include/ServiceUtil.hpp"
#include <crypt.h>
#include <cstring>

namespace hyperticket
{
    std::string hashPassword(const std::string &password)
    {
        std::string salt = generateBcryptSalt();
        struct crypt_data cd;
        std::memset(&cd, 0, sizeof(cd));
        const char *result = crypt_r(password.c_str(), salt.c_str(), &cd);
        return result ? std::string(result) : "";
    }

    bool verifyPassword(const std::string &password, const std::string &storedHash,
                        bool &needRehash)
    {
        needRehash = false;
        // bcrypt 哈希以 "$2b$" / "$2a$" / "$2y$" 开头
        if (storedHash.size() >= 4 && storedHash[0] == '$' &&
            (storedHash[1] == '2') && storedHash[3] == '$')
        {
            struct crypt_data cd;
            std::memset(&cd, 0, sizeof(cd));
            const char *result = crypt_r(password.c_str(), storedHash.c_str(), &cd);
            // 恒定时间比较，防止时序攻击
            if (!result) return false;
            return constantTimeEqual(std::string(result), storedHash);
        }
        // 旧 FNV-1a 哈希（无 "$" 开头）：兼容验证后标记需迁移
        // 恒定时间比较
        if (constantTimeEqual(fnv1aHash(password), storedHash))
        {
            needRehash = true;
            return true;
        }
        return false;
    }
} // namespace hyperticket
