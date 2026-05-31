// Buffer 单元测试：覆盖读写指针、retrieve/append、网络字节序、查找 EOL、扩容等纯逻辑。
#include "test_util.hpp"
#include "../backend/Inet/include/Buffer.hpp"
#include <sys/uio.h>

// Buffer::readFd 引用 Sockets::readv（定义在 SocketsOps.cpp，会拖入 Logger 整条依赖链）。
// 本测试不触碰 readFd，这里提供桩定义以满足链接，避免引入无关依赖。
namespace shanchuan { namespace Sockets {
    ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt) {
        return ::readv(sockfd, iov, iovcnt);
    }
}}

using shanchuan::Buffer;

static void test_initial_state()
{
    Buffer buf;
    CHECK_EQ(buf.readableBytes(), 0u);
    CHECK_EQ(buf.prependableBytes(), 8u); // KcheapPrepend
    CHECK_EQ(buf.writableBytes(), 4096u - 8u); // KinitialSize
}

static void test_append_retrieve()
{
    Buffer buf;
    const std::string data = "hello world";
    buf.append(data.data(), data.size());
    CHECK_EQ(buf.readableBytes(), data.size());
    CHECK_EQ(buf.writableBytes(), 4096u - 8u - data.size());

    std::string got = buf.retrieveAsString(5);
    CHECK_STR_EQ(got, "hello");
    CHECK_EQ(buf.readableBytes(), data.size() - 5);

    std::string rest = buf.retrieveAllAsString();
    CHECK_STR_EQ(rest, " world");
    CHECK_EQ(buf.readableBytes(), 0u);
    CHECK_EQ(buf.prependableBytes(), 8u); // retrieveAll 复位
}

static void test_retrieve_partial_moves_reader()
{
    Buffer buf;
    const std::string data = "abcdef";
    buf.append(data.data(), data.size());
    buf.retrieve(2);
    CHECK_EQ(buf.readableBytes(), 4u);
    CHECK_EQ(buf.prependableBytes(), 10u); // 8 + 2
    CHECK_STR_EQ(buf.retrieveAllAsString(), "cdef");
}

static void test_network_byte_order()
{
    Buffer buf;
    buf.appendInt32(0x01020304);
    buf.appendInt16(0x0506);
    buf.appendInt8(0x07);
    buf.appendInt64(0x0102030405060708LL);
    CHECK_EQ(buf.readableBytes(), 4u + 2u + 1u + 8u);

    CHECK_EQ(buf.readInt32(), 0x01020304);
    CHECK_EQ(buf.readInt16(), (int16_t)0x0506);
    CHECK_EQ(buf.readInt8(), (int8_t)0x07);
    CHECK_EQ(buf.readInt64(), 0x0102030405060708LL);
    CHECK_EQ(buf.readableBytes(), 0u);
}

static void test_peek_does_not_consume()
{
    Buffer buf;
    buf.appendInt32(12345);
    CHECK_EQ(buf.peekInt32(), 12345);
    CHECK_EQ(buf.readableBytes(), 4u); // peek 不消费
    CHECK_EQ(buf.readInt32(), 12345);
    CHECK_EQ(buf.readableBytes(), 0u);
}

static void test_find_eol()
{
    Buffer buf;
    const std::string data = "line1\nline2";
    buf.append(data.data(), data.size());
    const char *eol = buf.findEOL();
    CHECK(eol != nullptr);
    CHECK_EQ(eol - buf.peek(), 5); // '\n' 在索引 5

    Buffer buf2;
    const std::string noeol = "noeol";
    buf2.append(noeol.data(), noeol.size());
    CHECK(buf2.findEOL() == nullptr);
}

static void test_makespace_growth()
{
    Buffer buf;
    // 写入超过初始容量的数据，触发扩容
    std::string big(5000, 'x');
    buf.append(big.data(), big.size());
    CHECK_EQ(buf.readableBytes(), 5000u);
    CHECK_STR_EQ(buf.retrieveAllAsString(), big);
}

int main()
{
    RUN_TEST(test_initial_state);
    RUN_TEST(test_append_retrieve);
    RUN_TEST(test_retrieve_partial_moves_reader);
    RUN_TEST(test_network_byte_order);
    RUN_TEST(test_peek_does_not_consume);
    RUN_TEST(test_find_eol);
    RUN_TEST(test_makespace_growth);
    return TEST_SUMMARY();
}
