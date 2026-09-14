#include "../include/RedisConnPool.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include <chrono>
#include <stdexcept>

namespace hyperticket
{
    RedisConnPool::RedisConnPool(const std::string &host, int port, int poolSize)
        : host_(host), port_(port), poolSize_(poolSize)
    {
#ifdef USE_HIREDIS
        LOG_INFO << "Initializing Redis connection pool: " << host << ":" << port
                 << ", pool size: " << poolSize;

        for (int i = 0; i < poolSize_; ++i)
        {
            redisContext *ctx = createConnection();
            if (!ctx)
            {
                LOG_FATAL << "Failed to create Redis connection " << i;
                throw std::runtime_error("Redis connection pool initialization failed");
            }
            connList_.push_back(ctx);
        }

        LOG_INFO << "Redis connection pool initialized successfully: " << poolSize_ << " connections";
#else
        LOG_WARN << "Redis connection pool: hiredis not available, using placeholder";
        (void)host_;
        (void)port_;
        (void)poolSize_;
#endif
    }

    RedisConnPool::~RedisConnPool()
    {
#ifdef USE_HIREDIS
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto ctx : connList_)
        {
            if (ctx)
            {
                redisFree(ctx);
            }
        }
        connList_.clear();
        LOG_INFO << "Redis connection pool destroyed";
#endif
    }

#ifdef USE_HIREDIS
    redisContext *RedisConnPool::createConnection()
    {
        struct timeval timeout = {2, 0}; // 2 seconds timeout
        redisContext *ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);

        if (!ctx)
        {
            LOG_ERROR << "Redis connection failed: null context";
            return nullptr;
        }

        if (ctx->err)
        {
            LOG_ERROR << "Redis connection error: " << ctx->errstr;
            redisFree(ctx);
            return nullptr;
        }

        // 测试连接
        if (!ping(ctx))
        {
            LOG_ERROR << "Redis PING failed";
            redisFree(ctx);
            return nullptr;
        }

        return ctx;
    }

    bool RedisConnPool::reconnect(redisContext *ctx)
    {
        if (!ctx)
        {
            return false;
        }

        // 尝试重连
        if (redisReconnect(ctx) == REDIS_OK)
        {
            LOG_INFO << "Redis reconnected successfully";
            return ping(ctx);
        }

        LOG_ERROR << "Redis reconnect failed: " << ctx->errstr;
        return false;
    }

    bool RedisConnPool::ping(redisContext *ctx)
    {
        if (!ctx)
        {
            return false;
        }

        redisReply *reply = (redisReply *)redisCommand(ctx, "PING");
        if (!reply)
        {
            LOG_ERROR << "Redis PING failed: null reply";
            return false;
        }

        bool success = (reply->type == REDIS_REPLY_STATUS &&
                        std::string(reply->str) == "PONG");

        freeReplyObject(reply);
        return success;
    }

    redisContext *RedisConnPool::getConnection()
    {
        redisContext *ctx = nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_);

            // 限时等待可用连接：Redis 故障时不能让 worker 线程永久阻塞，
            // 超时抛异常由上层捕获并降级（回退直查 DB / 拒绝请求）。
            if (!cv_.wait_for(lock, std::chrono::milliseconds(kAcquireTimeoutMs),
                              [this] { return !connList_.empty() || lostSlots_ > 0; }))
            {
                LOG_ERROR << "Redis connection pool exhausted (waited "
                          << kAcquireTimeoutMs << "ms)";
                throw std::runtime_error("Redis connection acquire timeout");
            }

            if (!connList_.empty())
            {
                ctx = connList_.front();
                connList_.pop_front();
            }
            else
            {
                // 池中存在因故障丢失的名额：占用一个，锁外尝试重建连接
                --lostSlots_;
            }
        } // 健康检查/重连/建连较慢，放到锁外做，避免阻塞其他 worker 取连接

        if (!ctx)
        {
            ctx = createConnection();
            if (!ctx)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++lostSlots_; // 归还名额，等 Redis 恢复后再补
                throw std::runtime_error("Redis connection unavailable");
            }
            return ctx;
        }

        // 健康检查
        if (!ping(ctx))
        {
            LOG_WARN << "Redis connection unhealthy, attempting reconnect";
            if (!reconnect(ctx))
            {
                LOG_ERROR << "Redis reconnect failed, creating new connection";
                redisFree(ctx);
                ctx = createConnection();
                if (!ctx)
                {
                    // 注意：不能用 LOG_FATAL（会 abort 整个进程），Redis 挂掉时
                    // 抛异常让上层降级。登记 lostSlots_，后续 getConnection 在
                    // 池空时会尝试补建连接，Redis 恢复后池自动回满。
                    LOG_ERROR << "Failed to create new Redis connection";
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        ++lostSlots_;
                    }
                    cv_.notify_one();
                    throw std::runtime_error("Redis connection unavailable");
                }
            }
        }

        return ctx;
    }

    void RedisConnPool::releaseConnection(redisContext *ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        connList_.push_back(ctx);
        cv_.notify_one();
    }
#endif

} // namespace hyperticket
