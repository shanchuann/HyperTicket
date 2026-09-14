#ifndef HYPERTICKET_ISTOCK_CACHE_HPP
#define HYPERTICKET_ISTOCK_CACHE_HPP

#include <cstdint>
#include <string>

namespace hyperticket
{
    // 库存缓存接口：高并发下单场景中，Redis 原子预扣减挡在 MySQL 之前，
    // 让"无票"请求在缓存层秒拒，避免大量事务打到数据库行锁上。
    //
    // 一致性模型：MySQL 是库存的唯一真值（SELECT ... FOR UPDATE 事务不变），
    // 缓存只做"预筛"。任何缓存操作失败都必须降级为"直接走 DB"，不能阻塞下单。
    class IStockCache
    {
    public:
        virtual ~IStockCache() = default;

        // 预扣减结果
        enum class DecrResult
        {
            Ok,          // 扣减成功，可继续走 DB 事务
            SoldOut,     // 缓存显示无票，直接拒绝（不打 DB）
            Unavailable, // 缓存未命中或 Redis 故障，降级走 DB
        };

        // 原子预扣减 qty 张库存（Lua 保证 check-and-decr 原子性）。
        virtual DecrResult tryDecr(int64_t ticketId, int qty = 1) = 0;

        // 回补 1 张库存（DB 事务失败的补偿、取消订单的回补）。
        virtual void incr(int64_t ticketId, int delta = 1) = 0;

        // 用 DB 真值覆盖缓存（下单事务内锁行后校正、缓存未命中时回填）。
        virtual void set(int64_t ticketId, int availableSeats) = 0;

        // 在售列表缓存：命中返回 true 并填充 jsonOut（完整响应 JSON 串）。
        virtual bool getTicketList(std::string &jsonOut) = 0;
        virtual void setTicketList(const std::string &json, int ttlSeconds) = 0;

        // 失效列表缓存（票量/上下架变化后调用）。
        virtual void invalidateTicketList() = 0;
    };

    // 空实现：redis.enabled=false 时使用，一切行为等同"无缓存直连 DB"。
    class NoopStockCache : public IStockCache
    {
    public:
        DecrResult tryDecr(int64_t, int) override { return DecrResult::Unavailable; }
        void incr(int64_t, int) override {}
        void set(int64_t, int) override {}
        bool getTicketList(std::string &) override { return false; }
        void setTicketList(const std::string &, int) override {}
        void invalidateTicketList() override {}
    };
} // namespace hyperticket
#endif // HYPERTICKET_ISTOCK_CACHE_HPP
