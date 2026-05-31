# Inet 网络库

> 基于 epoll 的 Reactor 网络库，采用 muduo 风格的 one loop per thread 模型。
> 命名空间：`shanchuan`，C++17，仅依赖 Linux 系统调用与 STL（epoll / timerfd / eventfd / pthread）。

HyperTicket 的服务端 (`ser`) 即构建在本库之上，替代了早期版本使用的 libevent。

## 设计模型

- **one loop per thread**：每个线程持有一个 `EventLoop`，循环内通过 epoll 等待事件并分发回调。
- **主从 Reactor**：主线程的 `EventLoop` 负责 `Acceptor` 接受新连接，再通过 `EventLoopThreadPool` 以轮询方式把连接派发给 IO 线程，各自在自己的 loop 上处理读写。
- **连接生命周期**：`TcpConnection` 由 `shared_ptr` 管理；连接的所有回调都在其所属 loop 线程内执行，因此同一连接内无需加锁。

## 核心类

| 类 | 职责 |
|----|------|
| `EventLoop` | 事件循环核心：epoll 等待、执行定时器与待办函子，每线程一个 |
| `Channel` | 封装一个 fd 及其读/写/关闭/错误回调，向 `Poller` 注册关注事件 |
| `Poller` / `EPollPoller` | IO 多路复用抽象基类 / 基于 epoll 的实现 |
| `Acceptor` | 监听并接受新连接（`TcpServer` 内部使用） |
| `Connector` | 发起出站连接并带重试（`TcpClient` 内部使用） |
| `TcpServer` | 监听端口、管理连接生命周期、分发到 IO 线程池 |
| `TcpClient` | 发起出站连接，支持断线重连 |
| `TcpConnection` | 一条已建立连接：收发缓冲、各类回调 |
| `Buffer` | 应用层读写缓冲区，带读写下标与 prepend 区 |
| `InetAddress` | `sockaddr_in` 封装，IP/端口与网络结构互转 |
| `Socket` / `SocketsOps` | 底层 socket 封装（bind/listen/accept、TCP 选项等） |
| `EventLoopThread` / `EventLoopThreadPool` | 运行独立 loop 的线程 / 线程池 |
| `Timer` / `TimerQueue` / `TimerId` | 基于 timerfd 的定时器及其队列 |

## 回调类型（`Callbacks.hpp`）

```cpp
using TcpConnectionPtr      = std::shared_ptr<TcpConnection>;
using ConnectionCallback    = std::function<void(const TcpConnectionPtr)>;
using MessageCallback       = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;
using WriteComplateCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;
using CloseCallback         = std::function<void(const TcpConnectionPtr &)>;
using TimerCallback         = std::function<void()>;
```

## 常用 API

`EventLoop`
- `void loop()` / `void quit()`
- `void runInLoop(Functor)` / `void queueInLoop(Functor)`
- `TimerId runAt(Timestamp, TimerCallback)` / `runAfter(double, ...)` / `runEvery(double, ...)`
- `void cancel(TimerId)`、`bool isInLoopThread() const`

`TcpServer`
- `TcpServer(EventLoop *loop, const InetAddress &listenAddr, const string &name, Option = kNoReusePort)`
- `void setThreadNum(int)`
- `void setConnectionCallback(...)` / `setMessageCallback(...)` / `setWriteCompleteCallback(...)`
- `void start()`

`TcpConnection`
- `void send(const std::string &)` / `send(const void *, int)` / `send(Buffer *)`
- `void shutdown()` / `void forceClose()`
- `void setTcpNoDelay(bool)`、`void setContext(...)` / `std::any *getMutableContext()`
- `bool connected() const`、`const InetAddress &peerAddress() const`

## 最小示例（回显服务）

```cpp
#include "EventLoop.hpp"
#include "TcpServer.hpp"
#include "InetAddress.hpp"
using namespace shanchuan;

int main() {
    EventLoop loop;
    InetAddress addr("0.0.0.0", 8080);
    TcpServer server(&loop, addr, "EchoServer");
    server.setThreadNum(4); // 4 个 IO 线程

    server.setConnectionCallback([](const TcpConnectionPtr &c) {
        // c->connected() 为 true 表示连接建立，false 表示断开
    });
    server.setMessageCallback([](const TcpConnectionPtr &c, Buffer *buf, Timestamp) {
        c->send(buf->retrieveAllAsString()); // 原样回显
    });

    server.start();
    loop.loop();
}
```

实际用法（含按行拆包、限流、线程池派发业务）可参考 `Server/src/ser.cpp`。
