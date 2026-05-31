#ifndef HYPERTICKET_REDIS_CONN_POOL_HPP
#define HYPERTICKET_REDIS_CONN_POOL_HPP

#ifdef USE_HIREDIS
#include <hiredis/hiredis.h>
#endif

#include <string>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace hyperticket
{
    // Redis 连接池：管理 Redis 连接的获取、释放和健康检查
    class RedisConnPool
    {
    public:
        RedisConnPool(const std::string &host, int port, int poolSize);
        ~RedisConnPool();

        // 禁止拷贝和移动
        RedisConnPool(const RedisConnPool &) = delete;
        RedisConnPool &operator=(const RedisConnPool &) = delete;

#ifdef USE_HIREDIS
        // 获取连接（阻塞直到有可用连接）
        redisContext *getConnection();

        // 释放连接回池
        void releaseConnection(redisContext *ctx);

        // 健康检查：PING 命令
        bool ping(redisContext *ctx);
#endif

        // 获取连接池大小
        int getPoolSize() const { return poolSize_; }

    private:
#ifdef USE_HIREDIS
        // 创建新连接
        redisContext *createConnection();

        // 重连
        bool reconnect(redisContext *ctx);

        std::deque<redisContext *> connList_;
#endif
        std::string host_;
        int port_;
        int poolSize_;
        std::mutex mutex_;
        std::condition_variable cv_;
    };

#ifdef USE_HIREDIS
    // RAII 包装器：自动释放连接
    class RedisConnGuard
    {
    public:
        RedisConnGuard(redisContext **pctx, RedisConnPool *pool)
            : pctx_(pctx), pool_(pool)
        {
            *pctx_ = pool_->getConnection();
        }

        ~RedisConnGuard()
        {
            if (*pctx_)
            {
                pool_->releaseConnection(*pctx_);
                *pctx_ = nullptr;
            }
        }

        // 禁止拷贝和移动
        RedisConnGuard(const RedisConnGuard &) = delete;
        RedisConnGuard &operator=(const RedisConnGuard &) = delete;

    private:
        redisContext **pctx_;
        RedisConnPool *pool_;
    };
#endif

} // namespace hyperticket

#endif // HYPERTICKET_REDIS_CONN_POOL_HPP
