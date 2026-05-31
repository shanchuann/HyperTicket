#ifndef HYPERTICKET_REDIS_SESSION_MANAGER_HPP
#define HYPERTICKET_REDIS_SESSION_MANAGER_HPP

#include <string>
#include <cstdint>
#include <memory>
#include "../../Domain/include/ISessionManager.hpp"

namespace hyperticket
{
    class RedisConnPool;

    // Redis Session Manager：将 session 持久化到 Redis，支持多实例部署
    //
    // 设计要点：
    // 1. Key 格式：session:{token}
    // 2. Value 格式：JSON {tel, userId, expireMs}
    // 3. TTL：30分钟自动过期
    // 4. 连接池：复用 Redis 连接
    // 5. 错误处理：Redis 不可用时记录错误
    class RedisSessionManager : public ISessionManager
    {
    public:
        // redisHost: Redis 服务器地址
        // redisPort: Redis 端口
        // ttlMs: session 存活时长（毫秒）
        // poolSize: 连接池大小
        explicit RedisSessionManager(const std::string &redisHost = "127.0.0.1",
                                     int redisPort = 6379,
                                     int64_t ttlMs = 30 * 60 * 1000,
                                     int poolSize = 4);

        ~RedisSessionManager();

        // 生成并存储一个新 token 到 Redis
        std::string create(const std::string &tel, int64_t userId, int64_t nowMs) override;

        // 校验 token：从 Redis 读取，有效则续期
        bool resolve(const std::string &token, int64_t nowMs,
                     std::string &telOut, int64_t &userIdOut) override;

        // 定时清理过期 token（Redis 自动过期，此方法为空实现）
        void purgeExpired(int64_t nowMs) override;

        // 登出：从 Redis 删除 token
        void remove(const std::string &token);

        // 健康检查：测试 Redis 连接
        bool ping();

    private:
        // 生成随机 token（32字节，64个hex字符）
        std::string generateToken();

        // Redis 连接信息
        std::string redisHost_;
        int redisPort_;
        int64_t ttlMs_;

        // Redis 连接池
        std::unique_ptr<RedisConnPool> pool_;
    };

} // namespace hyperticket

#endif // HYPERTICKET_REDIS_SESSION_MANAGER_HPP
