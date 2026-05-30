#ifndef CALLBACK_Hpp
#define CALLBACK_Hpp
// C++ STL
#include <functional>
#include <memory>
using namespace std;

// OWNER
#include "Timestamp.hpp"

namespace shanchuan
{
    class TcpConnection;
    class Buffer;

    template <typename T>
    inline T *get_pointer(const std::shared_ptr<T> &ptr)
    {
        return ptr.get();
    }

    template <typename T>
    inline T *get_pointer(const std::unique_ptr<T> &ptr)
    {
        return ptr.get();
    }

    using TimerCallback = std::function<void(void)>;
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

    using WriteComplateCallback = std::function<void(const TcpConnectionPtr &)>;
    using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

    using ConnectionCallback = std::function<void(const TcpConnectionPtr)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, shanchuan::Timestamp)>;
    void defaultConnectionCallback(const TcpConnectionPtr &conn);

    void defaultMessageCallback(const TcpConnectionPtr &,
                                Buffer *buf,
                                shanchuan::Timestamp);
    // void defaultMessageCallback(const TcpConnectionPtr &,const std::string&,MicroTimestamp);
    using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
}

#endif