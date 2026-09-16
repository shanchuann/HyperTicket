#include "../include/ServiceUtil.hpp"
#include <crypt.h>
#include <cstring>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <vector>

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

    std::string sha256Hex(const std::string &value)
    {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(), digest);
        std::ostringstream out;
        for (unsigned char ch : digest) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        return out.str();
    }

    std::string encryptSensitive(const std::string &value)
    {
        const char *configured = std::getenv("HYPERTICKET_PII_KEY");
        const std::string secret = configured && *configured ? configured : "hyperticket-development-pii-key";
        unsigned char key[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char *>(secret.data()), secret.size(), key);
        unsigned char iv[16];
        if (RAND_bytes(iv, sizeof(iv)) != 1) return "";
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";
        std::vector<unsigned char> encrypted(value.size() + 32);
        int first = 0, last = 0;
        bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) == 1 &&
                  EVP_EncryptUpdate(ctx, encrypted.data(), &first,
                    reinterpret_cast<const unsigned char *>(value.data()), static_cast<int>(value.size())) == 1 &&
                  EVP_EncryptFinal_ex(ctx, encrypted.data() + first, &last) == 1;
        EVP_CIPHER_CTX_free(ctx);
        if (!ok) return "";
        std::ostringstream out;
        for (unsigned char ch : iv) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        for (int i = 0; i < first + last; ++i) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(encrypted[i]);
        return out.str();
    }

    std::string maskIdentity(const std::string &value)
    {
        if (value.size() <= 7) return value.size() <= 2 ? "**" : value.substr(0,1) + "***" + value.substr(value.size()-1);
        return value.substr(0,3) + std::string(value.size()-7, '*') + value.substr(value.size()-4);
    }
} // namespace hyperticket
