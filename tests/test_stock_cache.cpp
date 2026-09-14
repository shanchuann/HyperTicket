// Redis 库存缓存测试：原子预扣减、回补、真值校正、列表缓存。
// 依赖本机 Redis（127.0.0.1:6379）；连接失败时跳过（与 test_redis_session 一致）。

#include "../backend/Server/include/RedisStockCache.hpp"
#include "../backend/Server/include/RedisConnPool.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace hyperticket;

namespace
{
    const int64_t kTid = 999901; // 测试专用 ticket id，避免污染业务 key

    void testDecrBasics(RedisStockCache &cache)
    {
        std::cout << "Test 1: tryDecr basics... ";

        // 未初始化的 key：未命中，降级
        cache.incr(kTid, 100); // key 不存在时 incr 应为 no-op
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::Unavailable);

        // 初始化为 3：连续扣 3 次成功，第 4 次秒拒
        cache.set(kTid, 3);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::Ok);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::Ok);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::Ok);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::SoldOut);

        // 回补 1 张后又能扣
        cache.incr(kTid, 1);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::Ok);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::SoldOut);

        std::cout << "PASSED" << std::endl;
    }

    void testConcurrentDecr(RedisStockCache &cache)
    {
        std::cout << "Test 2: concurrent tryDecr (no oversell)... ";

        const int kStock = 50;
        const int kThreads = 8;
        const int kAttemptsPerThread = 40; // 总请求 320 >> 库存 50

        cache.set(kTid, kStock);

        std::atomic<int> okCount{0};
        std::vector<std::thread> workers;
        for (int i = 0; i < kThreads; ++i)
        {
            workers.emplace_back([&] {
                for (int j = 0; j < kAttemptsPerThread; ++j)
                {
                    if (cache.tryDecr(kTid) == IStockCache::DecrResult::Ok)
                        okCount.fetch_add(1);
                }
            });
        }
        for (auto &t : workers) t.join();

        // Lua 原子性保证：成功次数恰好等于库存，绝不超卖
        assert(okCount.load() == kStock);
        assert(cache.tryDecr(kTid) == IStockCache::DecrResult::SoldOut);

        std::cout << "PASSED (" << okCount.load() << "/" << kStock << ")" << std::endl;
    }

    void testTicketListCache(RedisStockCache &cache)
    {
        std::cout << "Test 3: ticket list cache... ";

        cache.invalidateTicketList();
        std::string out;
        assert(!cache.getTicketList(out));

        cache.setTicketList("{\"status\":\"OK\",\"num\":2}", 5);
        assert(cache.getTicketList(out));
        assert(out == "{\"status\":\"OK\",\"num\":2}");

        cache.invalidateTicketList();
        assert(!cache.getTicketList(out));

        std::cout << "PASSED" << std::endl;
    }
} // namespace

int main()
{
#ifdef USE_HIREDIS
    std::unique_ptr<RedisConnPool> pool;
    try
    {
        pool = std::make_unique<RedisConnPool>("127.0.0.1", 6379, 2);
    }
    catch (const std::exception &e)
    {
        std::cout << "Redis unavailable, skipping stock cache tests: " << e.what() << std::endl;
        return 0; // 无 Redis 环境视为跳过而非失败
    }

    RedisStockCache cache(pool.get());
    testDecrBasics(cache);
    testConcurrentDecr(cache);
    testTicketListCache(cache);

    std::cout << "All stock cache tests PASSED" << std::endl;
#else
    std::cout << "hiredis not available, stock cache tests skipped" << std::endl;
#endif
    return 0;
}
