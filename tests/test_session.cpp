// SessionManager 单元测试：create→resolve、错误 token、过期、续期、remove、purge。
#include "test_util.hpp"
#include "../backend/Server/include/SessionManager.hpp"

using hyperticket::SessionManager;

static void test_create_resolve()
{
    SessionManager sm(1000); // ttl 1000ms
    std::string token = sm.create("13800000000", 42, 0);
    CHECK(!token.empty());
    CHECK_EQ(token.size(), 64u); // 32 字节 -> 64 hex

    std::string tel;
    int64_t uid = 0;
    CHECK(sm.resolve(token, 100, tel, uid));
    CHECK_STR_EQ(tel, "13800000000");
    CHECK_EQ(uid, 42);
}

static void test_bad_token()
{
    SessionManager sm(1000);
    sm.create("13800000000", 1, 0);
    std::string tel;
    int64_t uid = 0;
    CHECK(!sm.resolve("not-a-real-token", 100, tel, uid));
}

static void test_expiry()
{
    SessionManager sm(1000);
    std::string token = sm.create("13811112222", 7, 0);
    std::string tel;
    int64_t uid = 0;
    CHECK(sm.resolve(token, 999, tel, uid));   // 未过期
    // 过期边界：expireMs <= now 即失效
    CHECK(!sm.resolve(token, 2000, tel, uid));  // 已过期
}

static void test_sliding_renewal()
{
    SessionManager sm(1000);
    std::string token = sm.create("13900000000", 9, 0);
    std::string tel;
    int64_t uid = 0;
    CHECK(sm.resolve(token, 500, tel, uid));   // 续期到 1500
    CHECK(sm.resolve(token, 1400, tel, uid));  // 因续期，1400 仍有效
}

static void test_remove()
{
    SessionManager sm(1000);
    std::string token = sm.create("13700000000", 3, 0);
    sm.remove(token);
    std::string tel;
    int64_t uid = 0;
    CHECK(!sm.resolve(token, 100, tel, uid));
}

static void test_purge()
{
    SessionManager sm(1000);
    sm.create("13700000001", 1, 0);
    sm.create("13700000002", 2, 0);
    CHECK_EQ(sm.size(), 2u);
    sm.purgeExpired(2000); // 全部过期
    CHECK_EQ(sm.size(), 0u);
}

int main()
{
    RUN_TEST(test_create_resolve);
    RUN_TEST(test_bad_token);
    RUN_TEST(test_expiry);
    RUN_TEST(test_sliding_renewal);
    RUN_TEST(test_remove);
    RUN_TEST(test_purge);
    return TEST_SUMMARY();
}
