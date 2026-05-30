#ifndef HYPERTICKET_TEST_UTIL_HPP
#define HYPERTICKET_TEST_UTIL_HPP

// 零依赖的微型单元测试工具：用宏记录通过/失败，main 返回非零表示有失败。
// 故意不引入 GoogleTest 等外部依赖，保持构建与 CI 简单。
#include <cstdio>
#include <cstring>
#include <string>

namespace httest
{
    inline int &failCount()
    {
        static int n = 0;
        return n;
    }
    inline int &checkCount()
    {
        static int n = 0;
        return n;
    }
}

// 基础布尔断言
#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        ++httest::checkCount();                                              \
        if (!(cond))                                                         \
        {                                                                    \
            ++httest::failCount();                                           \
            std::printf("  [FAIL] %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

// 相等断言（整型/可用 == 比较的类型）
#define CHECK_EQ(a, b)                                                       \
    do                                                                       \
    {                                                                        \
        ++httest::checkCount();                                              \
        if (!((a) == (b)))                                                   \
        {                                                                    \
            ++httest::failCount();                                           \
            std::printf("  [FAIL] %s:%d  CHECK_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b); \
        }                                                                    \
    } while (0)

// 字符串相等断言
#define CHECK_STR_EQ(a, b)                                                   \
    do                                                                       \
    {                                                                        \
        ++httest::checkCount();                                              \
        std::string _va = (a);                                               \
        std::string _vb = (b);                                               \
        if (_va != _vb)                                                      \
        {                                                                    \
            ++httest::failCount();                                           \
            std::printf("  [FAIL] %s:%d  CHECK_STR_EQ: \"%s\" != \"%s\"\n",  \
                        __FILE__, __LINE__, _va.c_str(), _vb.c_str());       \
        }                                                                    \
    } while (0)

#define RUN_TEST(fn)                                                         \
    do                                                                       \
    {                                                                        \
        std::printf("[ RUN  ] %s\n", #fn);                                   \
        fn();                                                                \
    } while (0)

// 在 main 末尾调用，打印汇总并返回退出码
#define TEST_SUMMARY()                                                       \
    (std::printf("\n%d checks, %d failures\n", httest::checkCount(), httest::failCount()), \
     httest::failCount() == 0 ? 0 : 1)

#endif // HYPERTICKET_TEST_UTIL_HPP
