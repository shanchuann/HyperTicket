// Redis Session Manager 集成测试
// 测试 Redis Session 的创建、解析、过期、续期等功能

#include "../backend/Server/include/RedisSessionManager.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>

using namespace hyperticket;

// 获取当前时间戳（毫秒）
int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void testBasicCreateAndResolve()
{
    std::cout << "Test 1: Basic create and resolve... ";

    RedisSessionManager mgr("127.0.0.1", 6379, 30 * 60 * 1000, 2);

    // 创建 session
    std::string token = mgr.create("13800138000", 1001, nowMs());
    assert(!token.empty());
    assert(token.length() == 64); // 32 bytes = 64 hex chars

    // 解析 session
    std::string tel;
    int64_t userId;
    bool ok = mgr.resolve(token, nowMs(), tel, userId);
    assert(ok);
    assert(tel == "13800138000");
    assert(userId == 1001);

    std::cout << "PASSED" << std::endl;
}

void testInvalidToken()
{
    std::cout << "Test 2: Invalid token... ";

    RedisSessionManager mgr("127.0.0.1", 6379, 30 * 60 * 1000, 2);

    std::string tel;
    int64_t userId;
    bool ok = mgr.resolve("invalid_token_12345678", nowMs(), tel, userId);
    assert(!ok);

    std::cout << "PASSED" << std::endl;
}

void testExpiration()
{
    std::cout << "Test 3: Session expiration... ";

    // 创建 TTL 为 2 秒的 session
    RedisSessionManager mgr("127.0.0.1", 6379, 2000, 2);

    int64_t now = nowMs();
    std::string token = mgr.create("13800138001", 1002, now);
    assert(!token.empty());

    // 立即解析应该成功
    std::string tel;
    int64_t userId;
    bool ok = mgr.resolve(token, now, tel, userId);
    assert(ok);

    // 等待 3 秒后应该过期
    std::this_thread::sleep_for(std::chrono::seconds(3));
    ok = mgr.resolve(token, nowMs(), tel, userId);
    assert(!ok);

    std::cout << "PASSED" << std::endl;
}

void testRenewal()
{
    std::cout << "Test 4: Session renewal... ";

    // 创建 TTL 为 3 秒的 session
    RedisSessionManager mgr("127.0.0.1", 6379, 3000, 2);

    int64_t now = nowMs();
    std::string token = mgr.create("13800138002", 1003, now);

    // 每隔 1 秒解析一次，应该持续续期
    for (int i = 0; i < 5; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string tel;
        int64_t userId;
        bool ok = mgr.resolve(token, nowMs(), tel, userId);
        assert(ok);
        assert(tel == "13800138002");
    }

    std::cout << "PASSED" << std::endl;
}

void testRemove()
{
    std::cout << "Test 5: Session removal... ";

    RedisSessionManager mgr("127.0.0.1", 6379, 30 * 60 * 1000, 2);

    std::string token = mgr.create("13800138003", 1004, nowMs());
    assert(!token.empty());

    // 删除 session
    mgr.remove(token);

    // 再次解析应该失败
    std::string tel;
    int64_t userId;
    bool ok = mgr.resolve(token, nowMs(), tel, userId);
    assert(!ok);

    std::cout << "PASSED" << std::endl;
}

void testConcurrency()
{
    std::cout << "Test 6: Concurrent access... ";

    RedisSessionManager mgr("127.0.0.1", 6379, 30 * 60 * 1000, 4);

    // 创建多个 session
    std::vector<std::string> tokens;
    for (int i = 0; i < 10; ++i)
    {
        std::string tel = "1380013800" + std::to_string(i);
        std::string token = mgr.create(tel, 2000 + i, nowMs());
        tokens.push_back(token);
    }

    // 并发解析
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 10; ++i)
    {
        std::string token = tokens[i];
        int64_t expectedUserId = 2000 + i;
        threads.emplace_back([&mgr, token, expectedUserId]()
                             {
            for (int j = 0; j < 10; ++j) {
                std::string tel;
                int64_t userId;
                bool ok = mgr.resolve(token, nowMs(), tel, userId);
                assert(ok);
                assert(userId == expectedUserId);
            } });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    std::cout << "PASSED" << std::endl;
}

void testPing()
{
    std::cout << "Test 7: Health check (ping)... ";

    RedisSessionManager mgr("127.0.0.1", 6379, 30 * 60 * 1000, 2);
    bool ok = mgr.ping();
    assert(ok);

    std::cout << "PASSED" << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "Redis Session Manager Integration Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    try
    {
        testBasicCreateAndResolve();
        testInvalidToken();
        testExpiration();
        testRenewal();
        testRemove();
        testConcurrency();
        testPing();

        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "All tests PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << std::endl;
        std::cerr << "Test FAILED: " << e.what() << std::endl;
        std::cerr << std::endl;
        std::cerr << "Note: If Redis is not installed or not running," << std::endl;
        std::cerr << "      tests will use file-based placeholder." << std::endl;
        std::cerr << "      Install Redis: sudo apt install redis-server" << std::endl;
        std::cerr << "      Install hiredis: sudo apt install libhiredis-dev" << std::endl;
        return 1;
    }
}
