#ifndef HYPERTICKET_REDIS_STOCK_CACHE_HPP
#define HYPERTICKET_REDIS_STOCK_CACHE_HPP

#include <cstdint>
#include <string>

#include "../../Domain/include/IStockCache.hpp"

namespace hyperticket
{
    class RedisConnPool;

    // Redis 库存缓存实现：
    // - Key：stock:{ticketId}（string，剩余库存数）
    // - 预扣减：Lua 脚本原子执行 "存在且 >0 则 DECR"，杜绝竞态超卖
    // - 列表缓存：cache:tickets:onsale（完整响应 JSON，短 TTL）
    // - 全部操作 try/catch：Redis 故障时降级（Unavailable / no-op），服务不受影响
    class RedisStockCache : public IStockCache
    {
    public:
        // pool 由外部持有（与 RedisSessionManager 共享连接池）
        explicit RedisStockCache(RedisConnPool *pool) : pool_(pool) {}

        DecrResult tryDecr(int64_t ticketId, int qty = 1) override;
        void incr(int64_t ticketId, int delta = 1) override;
        void set(int64_t ticketId, int availableSeats) override;

        bool getTicketList(std::string &jsonOut) override;
        void setTicketList(const std::string &json, int ttlSeconds) override;
        void invalidateTicketList() override;

    private:
        RedisConnPool *pool_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_REDIS_STOCK_CACHE_HPP
