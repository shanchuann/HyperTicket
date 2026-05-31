#include "../include/RedisConnPool.hpp"
#include "../../ChronoLite/include/Logger.hpp"
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
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待可用连接
        cv_.wait(lock, [this]
                 { return !connList_.empty(); });

        redisContext *ctx = connList_.front();
        connList_.pop_front();

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
                    LOG_FATAL << "Failed to create new Redis connection";
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
