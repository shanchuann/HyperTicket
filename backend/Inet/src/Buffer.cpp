#include <errno.h>
#include <sys/uio.h>
#include <memory>
#include "Buffer.hpp"
#include "SocketsOps.hpp"

namespace shanchuan
{
    const char Buffer::CRLF[] = "\r\n";
    const size_t Buffer::KcheapPrepend;
    const size_t Buffer::KinitialSize;

    ssize_t Buffer::readFd(int fd, int *savedErrno)
    {
        char extrabuf[65536];                                   // 栈上的内存空间 64K
        // std::unique_ptr<char[]> extra(new char[1024 * 64]);  // 堆上的内存空间 64K
        struct iovec vec[2];
        const size_t writable = writableBytes();                // buffer可写的数据
        vec[0].iov_base = begin() + writerIndex_;
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;                             // vec[1].iov_base = extra.get();
        vec[1].iov_len = sizeof extrabuf;                       // vec[1].iov_len = 1024 * 64;
        
        // 当writable空间不足以存储读出来的数据时, 通过readv将数据读到buffer_的writable空间和extrabuf中
        // readv的返回值n表示读出来的字节数, 可能会超过buffer_的writable空间大小, 但不会超过writable + sizeof extrabuf
        const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
        const ssize_t n = Sockets::readv(fd, vec, iovcnt);
        if (n < 0) *savedErrno = errno; // :readv系统调用错误
        else if (static_cast<size_t>(n) <= writable) writerIndex_ += n; // Buffer的可写缓冲区已经够存储读出来的数据了
        else { 
            // 读取的数据超过现有内部buffer_的writable空间大小时, 启用备用的extrabuf 64KB空间, 并将这些数据添加到内部buffer_的末尾
            // 过程可能会合并多余prependable空间或resize buffer_大小, 以腾出足够writable空间存放数据
            writerIndex_ = buffer_.size();
            append(extrabuf, n - writable);                     // append(extra.get(), n - writable);
        }
        return n;
    }
} // namespace shanchuan