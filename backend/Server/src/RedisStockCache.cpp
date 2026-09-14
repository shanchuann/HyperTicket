#include "../include/RedisStockCache.hpp"
#include "../include/RedisConnPool.hpp"
#include "../../ChronoLite/include/Logger.hpp"

#ifdef USE_HIREDIS
#include <hiredis/hiredis.h>
#endif

namespace hyperticket
{
#ifdef USE_HIREDIS
    namespace
    {
        const char *kTicketListKey = "cache:tickets:onsale";

        // 原子预扣减 Lua 脚本（EVAL 在 Redis 内单线程执行，check-and-decr 无竞态）：
        //   ARGV[1] = 要扣的张数 qty
        //   返回 -1：key 不存在（缓存未命中，降级走 DB 并回填）
        //   返回 -2：库存不足 qty（秒拒，不打 DB）
        //   返回 >=0：DECRBY 成功后的剩余库存（0 表示抢到最后一批，仍是成功）
        const char *kDecrScript =
            "local v = redis.call('GET', KEYS[1]) "
            "if not v then return -1 end "
            "v = tonumber(v) "
            "local q = tonumber(ARGV[1]) "
            "if v < q then return -2 end "
            "return redis.call('DECRBY', KEYS[1], q)";

        std::string stockKey(int64_t ticketId)
        {
            return "stock:" + std::to_string(ticketId);
        }
    }

    IStockCache::DecrResult RedisStockCache::tryDecr(int64_t ticketId, int qty)
    {
        if (ticketId <= 0 || qty <= 0)
            return DecrResult::Unavailable;
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_);

            std::string key = stockKey(ticketId);
            redisReply *reply = (redisReply *)redisCommand(
                ctx, "EVAL %s 1 %s %d", kDecrScript, key.c_str(), qty);

            if (!reply || reply->type != REDIS_REPLY_INTEGER)
            {
                if (reply)
                    freeReplyObject(reply);
                LOG_WARN << "Stock cache tryDecr unexpected reply, fallback to DB";
                return DecrResult::Unavailable;
            }

            long long v = reply->integer;
            freeReplyObject(reply);

            if (v == -1)
                return DecrResult::Unavailable; // 未命中，走 DB 并回填
            if (v == -2)
                return DecrResult::SoldOut;     // 库存不足秒拒
            return DecrResult::Ok;              // v >= 0，扣减成功
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Stock cache tryDecr failed: " << e.what();
            return DecrResult::Unavailable;
        }
    }

    void RedisStockCache::incr(int64_t ticketId, int delta)
    {
        if (ticketId <= 0 || delta <= 0)
            return;
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_);

            std::string key = stockKey(ticketId);
            // 仅在 key 存在时回补，否则会把未初始化的库存凭空 INCR 出来
            redisReply *reply = (redisReply *)redisCommand(
                ctx,
                "EVAL %s 1 %s %d",
                "if redis.call('EXISTS', KEYS[1]) == 1 then "
                "return redis.call('INCRBY', KEYS[1], ARGV[1]) end return -1",
                key.c_str(), delta);
            if (reply)
                freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            // 回补失败不致超卖（缓存偏小只会多放请求去 DB，DB 行锁兜底），仅记日志
            LOG_ERROR << "Stock cache incr failed: " << e.what();
        }
    }

    void RedisStockCache::set(int64_t ticketId, int availableSeats)
    {
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_);

            std::string key = stockKey(ticketId);
            // 异步下单模式下库存 key 不能自然过期：队列尚未落库时若按 DB
            // 真值重建，会忘掉已排队的预扣减。下架时由业务显式置零。
            redisReply *reply = (redisReply *)redisCommand(
                ctx, "SET %s %d", key.c_str(), availableSeats);
            if (reply)
                freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Stock cache set failed: " << e.what();
        }
    }

    bool RedisStockCache::getTicketList(std::string &jsonOut)
    {
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_);

            redisReply *reply = (redisReply *)redisCommand(
                ctx, "GET %s", kTicketListKey);
            if (!reply || reply->type != REDIS_REPLY_STRING)
            {
                if (reply)
                    freeReplyObject(reply);
                return false;
            }
            jsonOut.assign(reply->str, reply->len);
            freeReplyObject(reply);
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Stock cache getTicketList failed: " << e.what();
            return false;
        }
    }

    void RedisStockCache::setTicketList(const std::string &json, int ttlSeconds)
    {
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_);

            redisReply *reply = (redisReply *)redisCommand(
                ctx, "SETEX %s %d %b", kTicketListKey, ttlSeconds,
                json.data(), json.size());
            if (reply)
                freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Stock cache setTicketList failed: " << e.what();
        }
    }

    void RedisStockCache::invalidateTicketList()
    {
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_);

            redisReply *reply = (redisReply *)redisCommand(
                ctx, "DEL %s", kTicketListKey);
            if (reply)
                freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Stock cache invalidateTicketList failed: " << e.what();
        }
    }
#else
    // hiredis 不可用：占位实现，全部降级
    IStockCache::DecrResult RedisStockCache::tryDecr(int64_t, int)
    {
        return DecrResult::Unavailable;
    }
    void RedisStockCache::incr(int64_t, int) {}
    void RedisStockCache::set(int64_t, int) {}
    bool RedisStockCache::getTicketList(std::string &) { return false; }
    void RedisStockCache::setTicketList(const std::string &, int) {}
    void RedisStockCache::invalidateTicketList() {}
#endif
} // namespace hyperticket
