// Timestamp 单元测试：覆盖 valid/invalid、构造、秒/微秒换算、diff、addTime。
// 注意：toFormattedString 依赖本地时区，这里只校验时区无关的逻辑与字符串结构。
#include "test_util.hpp"
#include "../ChronoLite/include/Timestamp.hpp"

using logsys::Timestamp;

static void test_valid_invalid()
{
    Timestamp def;            // 默认构造 microSec=0，无效
    CHECK(!def.valid());

    Timestamp inv = Timestamp::invalid();
    CHECK(!inv.valid());

    Timestamp t(1000000LL);   // 1 秒
    CHECK(t.valid());
}

static void test_seconds_micros()
{
    // 1234567890 秒 + 654321 微秒
    int64_t micros = 1234567890LL * Timestamp::kMicroSecPerSecond + 654321LL;
    Timestamp t(micros);
    CHECK_EQ(t.getMicroSec(), micros);
    CHECK_EQ((int64_t)t.getSeconds(), 1234567890LL);
}

static void test_diff()
{
    Timestamp low(5LL * Timestamp::kMicroSecPerSecond);
    Timestamp high(8LL * Timestamp::kMicroSecPerSecond + 500000LL);
    CHECK_EQ((int64_t)logsys::diffSeconds(high, low), 3LL);
    CHECK_EQ(logsys::diffMicroSeconds(high, low), 3500000LL);
}

static void test_add_time()
{
    Timestamp base(10LL * Timestamp::kMicroSecPerSecond);
    Timestamp later = logsys::addTime(base, 2.5);
    CHECK_EQ(later.getMicroSec(), 12500000LL);
}

static void test_now_is_valid_and_increases()
{
    Timestamp a = Timestamp::Now();
    CHECK(a.valid());
    // Now() 应落在一个合理的现代时间范围内（> 2020-01-01）
    CHECK(a.getSeconds() > 1577836800LL);
}

static void test_formatted_string_structure()
{
    // 任意固定时间戳；只校验格式化串长度与分隔符结构，不校验具体时区值。
    Timestamp t(1234567890LL * Timestamp::kMicroSecPerSecond + 123456LL);
    std::string withMic = t.toFormattedString(true);   // YYYY/MM/DD HH:MM:SS.uuuuuu
    std::string noMic = t.toFormattedString(false);    // YYYY/MM/DD HH:MM:SS
    CHECK(withMic.size() > noMic.size());
    CHECK(withMic.find('/') != std::string::npos);
    CHECK(withMic.find(':') != std::string::npos);
    CHECK(noMic.find('.') == std::string::npos);
}

int main()
{
    RUN_TEST(test_valid_invalid);
    RUN_TEST(test_seconds_micros);
    RUN_TEST(test_diff);
    RUN_TEST(test_add_time);
    RUN_TEST(test_now_is_valid_and_increases);
    RUN_TEST(test_formatted_string_structure);
    return TEST_SUMMARY();
}
