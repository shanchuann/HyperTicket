#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <algorithm>
#include <vector>
#include <string>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "Endian.hpp"

namespace shanchuan
{
    class Buffer
    {
    private:
        std::vector<char> buffer_;
        size_t readerIndex_;        // 可读数据首地址
        size_t writerIndex_;        // 可写数据首地址
        static const char CRLF[];   // "\r\n"
        
        char *begin() { return &*buffer_.begin(); } // 返回缓冲区的起始位置, 也是prependable空间起始位置
        const char *begin() const { return &*buffer_.begin(); }
        void makeSpace(size_t len) {
            if (writableBytes() + prependableBytes() < len + KcheapPrepend) {
                // writable 空间大小 + prependable空间大小 不足以存放len byte数据,
                // resize内部缓冲区大小
                buffer_.resize(writerIndex_ + len);
            } else {
                // writable 空间大小 + prependable空间大小 足以存放len byte数据,
                // 移动readable空间数据, 合并多余prependable空间到writable空间
                assert(KcheapPrepend < readerIndex_);
                size_t readable = readableBytes();
                std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + KcheapPrepend);
                readerIndex_ = KcheapPrepend;
                writerIndex_ = readerIndex_ + readable;
                assert(readable == readableBytes());
            }
        }
    public:
        static const size_t KcheapPrepend = 8;                           // 头部空间预留空间大小
        static const size_t KinitialSize = (1024 * 4 - KcheapPrepend);   // Buffer初始大小
        explicit Buffer(size_t initialSize = KinitialSize): buffer_(KcheapPrepend + initialSize), readerIndex_(KcheapPrepend), writerIndex_(KcheapPrepend) {
            assert(readableBytes() == 0);
            assert(writableBytes() == initialSize);
            assert(prependableBytes() == KcheapPrepend);
        }
        // 交换buffer_、readerIndex_和writerIndex_的值
        void swap(Buffer &rhs) {
            buffer_.swap(rhs.buffer_);
            std::swap(readerIndex_, rhs.readerIndex_);
            std::swap(writerIndex_, rhs.writerIndex_);
        }
        size_t readableBytes() const { return writerIndex_ - readerIndex_; }    // 可读字节数          
        size_t writableBytes() const { return buffer_.size() - writerIndex_; }  // 可写字节数  
        size_t prependableBytes() const { return readerIndex_; }                // 预留字节数
        const char *peek() const { return begin() + readerIndex_; }             // 返回指向可读空间的起始位置指针
        // 在start到writable首地址 之间找CRLF, 要求start在readable地址空间中 CR = '\r', LF = '\n'
        const char *findCRLF() const {
            const char *crlf = std::search(peek(), beginWrite(), CRLF, CRLF + 2);
            return crlf == beginWrite() ? NULL : crlf;
        }
        const char *findCRLF(const char *start) const {
            assert(peek() <= start);
            assert(start <= beginWrite());
            const char *crlf = std::search(start, beginWrite(), CRLF, CRLF + 2);
            return crlf == beginWrite() ? NULL : crlf;
        }
        // 在readable空间中找EOL, 即LF('\n')
        const char *findEOL() const {
            const void *eol = memchr(peek(), '\n', readableBytes());
            return static_cast<const char *>(eol);
        }
        // 在start到writable首地址 之间找EOL, 要求start在readable地址空间中
        const char *findEOL(const char *start) const {
            assert(peek() <= start);
            assert(start <= beginWrite());
            const void *eol = memchr(start, '\n', beginWrite() - start);
            return static_cast<const char *>(eol);
        }
        // 将缓冲区len的长度进行复位 从readable头部取走最多长度为len byte的数据. 会导致readable空间变化, 可能导致writable空间变化.
        // 这里取走只是移动readerIndex_, writerIndex_, 并不会直接读取或清除readable, writable空间数据　
        void retrieve(size_t len) {
            assert(len <= readableBytes());
            if (len < readableBytes()) readerIndex_ += len; // 表示 应用只读取了刻度缓冲区数据的一部分，就是len， 还剩下readerIndex_ += len -> writerIndex_
            else retrieveAll(); // 全部取走
        }
        // end前的数据全取走 从readable空间取走 [peek(), end)这段区间数据, peek()是readable空间首地址
        void retrieveUntil(const char *end) {
            assert(peek() <= end);
            assert(end <= beginWrite());
            retrieve(end - peek());
        }
        void retrieveInt64() { retrieve(sizeof(int64_t)); } // 从readable空间取走一个int64_t数据, 长度8byte   
        void retrieveInt32() { retrieve(sizeof(int32_t)); } // 从readable空间取走一个int32_t数据, 长度4byte  
        void retrieveInt16() { retrieve(sizeof(int16_t)); } // 从readable空间取走一个int16_t数据, 长度2byte    
        void retrieveInt8()  { retrieve(sizeof(int8_t));  } // 从readable空间取走一个int8_t数据, 长度1byte  
        void retrieveAll()   { // 从readable空间取走所有数据, 直接移动readerIndex_, writerIndex_指示器即可
            readerIndex_ = KcheapPrepend;
            writerIndex_ = KcheapPrepend;
        }     
        std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); } // 把onMessage函数上报的buffer内容转为string
        std::string retrieveAsString(size_t len) { // 从可读数据开始位置，长度为len的char构造为一个string
            assert(len <= readableBytes());
            std::string result(peek(), len);
            retrieve(len);
            return result;
        }
        void append(const char * /*restrict*/ data, size_t len) { // 向缓冲区添加数据
            ensureWritableBytes(len);
            std::copy(data, data + len, beginWrite());
            hasWritten(len);
        }
        void append(const void * /*restrict*/ data, size_t len) { append(static_cast<const char *>(data), len); }       
        void ensureWritableBytes(size_t len) {          // 要写len长度的数据
            if (writableBytes() < len) makeSpace(len);  // 扩容函数
            assert(writableBytes() >= len);
        }
        char *beginWrite() { return begin() + writerIndex_; } // 返回待写入数据的地址, 即writable空间首地址
        const char *beginWrite() const { return begin() + writerIndex_; }
        void hasWritten(size_t len) {   // 将writerIndex_往后移动len byte, 需要确保writable空间足够大
            assert(len <= writableBytes());
            writerIndex_ += len;
        }
        void unwrite(size_t len) {      // 将writerIndex_往前移动len byte, 需要确保readable空间足够大
            assert(len <= readableBytes());
            writerIndex_ -= len;
        }
        void appendInt64(int64_t x) {   // 添加一个int64_t类型的数x到缓冲区末尾, 以网络字节序形式存储
            int64_t be64 = Sockets::hostToNetwork64(x);
            append(&be64, sizeof be64);
        }
        void appendInt32(int32_t x) {   // 接受一个 int32_t 类型的参数，并将其以网络字节序（big-endian）的形式追加到 Buffer 的末尾
            int32_t be32 = Sockets::hostToNetwork32(x);
            append(&be32, sizeof be32);
        }
        void appendInt16(int16_t x) {   // 接受一个 int16_t 类型的参数，并将其以网络字节序（big-endian）的形式追加到 Buffer 的末尾
            int16_t be16 = Sockets::hostToNetwork16(x);
            append(&be16, sizeof be16);
        }
        void appendInt8(int8_t x)   {   // 接受一个 int8_t 类型的参数，并将其以网络字节序（big-endian）的形式追加到 Buffer 的末尾. 1byte数据不存在字节序问题
            int8_t be8 = x;
            append(&be8, sizeof be8);
        }
        int64_t readInt64() {       // 从readable空间头部读取一个int64_类型数, 由网络字节序转换为本地字节序后返回, 读取完成后会导致readable空间变化, 可能导致writable空间变化
            int64_t result = peekInt64();
            retrieveInt64();
            return result;
        }
        int32_t readInt32() {       // 从readable空间头部读取一个int32_类型数, 由网络字节序转换为本地字节序后返回, 读取完成后会导致readable空间变化, 可能导致writable空间变化
            int32_t result = peekInt32();
            retrieveInt32();
            return result;
        }
        int16_t readInt16() {       // 从readabl e空间头部读取一个int16_类型数, 由网络字节序转换为本地字节序后返回, 读取完成后会导致readable空间变化, 可能导致writable空间变化
            int16_t result = peekInt16();
            retrieveInt16();
            return result;
        }
        int8_t readInt8()   {       // 从readable空间头部读取一个int8_类型数, 由网络字节序转换为本地字节序后返回, 读取完成后会导致readable空间变化, 可能导致writable空间变化. 1byte数据不存在字节序问题
            int8_t result = peekInt8();
            retrieveInt8();
            return result;
        }
        int64_t peekInt64() const { // 从readable的头部peek()读取一个int64_t数据, 但不移动readerIndex_, 不会改变readable空间
            assert(readableBytes() >= sizeof(int64_t));
            int64_t be64 = 0;
            ::memcpy(&be64, peek(), sizeof be64);
            return Sockets::networkToHost64(be64);
        }
        int32_t peekInt32() const { // 从readable的头部peek()读取一个int32_t数据, 但不移动readerIndex_, 不会改变readable空间
            assert(readableBytes() >= sizeof(int32_t));
            int32_t be32 = 0;
            ::memcpy(&be32, peek(), sizeof be32);
            return Sockets::networkToHost32(be32);
        }
        int16_t peekInt16() const { // 从readable的头部peek()读取一个int16_t数据, 但不移动readerIndex_, 不会改变readable空间
            assert(readableBytes() >= sizeof(int16_t));
            int16_t be16 = 0;
            ::memcpy(&be16, peek(), sizeof be16);
            return Sockets::networkToHost16(be16);
        }
        int8_t peekInt8() const { // 从readable的头部peek()读取一个int8_t数据, 但不移动readerIndex_, 不会改变readable空间. * 1byte数据不存在字节序问题
            assert(readableBytes() >= sizeof(int8_t));
            int8_t x = *peek();
            return x;
        }
        void prependInt64(int64_t x) { // 在prependable空间末尾预置int64_t类型网络字节序的数x, 预置数会被转化为本地字节序
            int64_t be64 = Sockets::hostToNetwork64(x);
            prepend(&be64, sizeof be64);
        }
        void prependInt32(int32_t x) { // 在prependable空间末尾预置int32_t类型网络字节序的数x, 预置数会被转化为本地字节序
            int32_t be32 = Sockets::hostToNetwork32(x);
            prepend(&be32, sizeof be32);
        }
        void prependInt16(int16_t x) { // 在prependable空间末尾预置int16_t类型网络字节序的数x, 预置数会被转化为本地字节序
            int16_t be16 = Sockets::hostToNetwork16(x);
            prepend(&be16, sizeof be16);
        }
        void prependInt8(int8_t x)  { // 在prependable空间末尾预置int8_t类型网络字节序的数x, 预置数会被转化为本地字节序. 1byte数据不存在字节序问题
            int8_t be8 = x;
            prepend(&be8, sizeof be8);
        }
        void prepend(const void * /*restrict*/ data, size_t len) { // 在prependable空间末尾预置一组二进制数据data[len]. 表面上看是加入prependable空间末尾, 实际上是加入readable开头, 会导致readerIndex_变化
            assert(len <= prependableBytes());
            readerIndex_ -= len;
            const char *d = static_cast<const char *>(data);
            std::copy(d, d + len, begin() + readerIndex_);
        }
        void shrink(size_t reserve) { // 在prependable空间末尾预置一组二进制数据data[len]. 表面上看是加入prependable空间末尾, 实际上是加入readable开头, 会导致readerIndex_变化
            // shrink to fit, 使得buffer_的capacity()和size()相等, 但保留reserve字节的可写空间
            Buffer other;
            other.ensureWritableBytes(readableBytes() + reserve);
            swap(other);
        }
        size_t internalCapacity() const { return buffer_.capacity(); }  // 返回buffer_的容量capacity()
        ssize_t readFd(int fd, int *savedErrno);
        ssize_t writeFd(int fd, int *saveErrno) {                       // 通过fd发送数据
            ssize_t n = ::write(fd, peek(), readableBytes());           // 直接将可读的数据发送给出去就行
            if (n < 0) *saveErrno = errno;
            return n;
        }
    };
} // namespace shanchuan
#endif // BUFFER_HPP