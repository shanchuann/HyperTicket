#ifndef HYPERTICKET_SESSION_MANAGER_HPP
#define HYPERTICKET_SESSION_MANAGER_HPP

#include <cstdint>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

#include "../../Domain/include/ISessionManager.hpp"

namespace hyperticket
{
    // 服务端登录会话表：登录成功后签发随机 token，后续请求凭 token 反查用户身份，
    // 服务端不再信任客户端自报的 tel，从而杜绝越权。线程安全（worker 线程并发访问）。
    class SessionManager : public ISessionManager
    {
    public:
        // ttlMs: token 存活时长；nowMs: 可注入的时钟（默认用系统时钟），便于测试过期逻辑。
        explicit SessionManager(int64_t ttlMs = 30 * 60 * 1000)
            : ttlMs_(ttlMs) {}

        // 生成并存储一个新 token，返回该 token。
        std::string create(const std::string &tel, int64_t userId, int64_t nowMs) override
        {
            std::string token = generateToken();
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_[token] = Session{tel, userId, nowMs + ttlMs_};
            return token;
        }

        // 校验 token：有效则填充 tel/userId 并续期，返回 true；无效或过期返回 false。
        bool resolve(const std::string &token, int64_t nowMs,
                     std::string &telOut, int64_t &userIdOut) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = sessions_.find(token);
            if (it == sessions_.end())
            {
                return false;
            }
            if (it->second.expireMs <= nowMs)
            {
                sessions_.erase(it);
                return false;
            }
            telOut = it->second.tel;
            userIdOut = it->second.userId;
            it->second.expireMs = nowMs + ttlMs_; // 滑动续期
            return true;
        }

        // 登出：移除 token。
        void remove(const std::string &token)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_.erase(token);
        }

        // 定时清理过期 token。
        void purgeExpired(int64_t nowMs) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto it = sessions_.begin(); it != sessions_.end();)
            {
                if (it->second.expireMs <= nowMs)
                {
                    it = sessions_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        size_t size()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return sessions_.size();
        }

    private:
        struct Session
        {
            std::string tel;
            int64_t userId;
            int64_t expireMs;
        };

        // 生成 32 字节（64 hex 字符）随机 token。优先用 /dev/urandom，回退到 random_device。
        std::string generateToken()
        {
            unsigned char buf[32];
            std::ifstream urandom("/dev/urandom", std::ios::binary);
            if (urandom.good() && urandom.read(reinterpret_cast<char *>(buf), sizeof(buf)))
            {
                return toHex(buf, sizeof(buf));
            }
            // 回退：random_device（仍为加密级别的实现在多数平台）
            std::random_device rd;
            for (size_t i = 0; i < sizeof(buf); ++i)
            {
                buf[i] = static_cast<unsigned char>(rd() & 0xFF);
            }
            return toHex(buf, sizeof(buf));
        }

        static std::string toHex(const unsigned char *data, size_t len)
        {
            static const char *hex = "0123456789abcdef";
            std::string out;
            out.reserve(len * 2);
            for (size_t i = 0; i < len; ++i)
            {
                out.push_back(hex[data[i] >> 4]);
                out.push_back(hex[data[i] & 0x0F]);
            }
            return out;
        }

        std::unordered_map<std::string, Session> sessions_;
        std::mutex mutex_;
        int64_t ttlMs_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_SESSION_MANAGER_HPP
