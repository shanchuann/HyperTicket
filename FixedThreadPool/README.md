# FixedThreadPool 固定线程池

> 固定大小线程池 + 有界阻塞同步队列，纯头文件，C++17，命名空间 `shanchuan`。
> 头文件：`FixedThreadPool.hpp`、`SyncQueue.hpp`。

构造时即创建固定数量的工作线程，循环从内部的 `SyncQueue` 取任务执行，直到 `stop()`。
队列满或正在停止时，任务会回退到调用者线程内同步执行，避免任务丢失。

HyperTicket 服务端用它承载业务处理：IO 线程解析完一条请求后，把数据库操作投递到此线程池，避免阻塞事件循环。

## 公共 API

```cpp
// 构造：默认线程数为硬件并发数
explicit FixedThreadPool(int num_threads = std::thread::hardware_concurrency());

// 投递无返回值任务（Task = std::function<void()>）
void add_task(const Task &task);
void add_task(Task &&task);

// 投递带返回值任务，返回 std::future 以获取结果
template <typename F, typename... Args>
auto submit(F &&f, Args &&...args) -> std::future<...>;

// 优雅停止：停止取任务、唤醒并 join 所有线程（内部 call_once 保证只执行一次）
void stop();
```

内部 `SyncQueue<T>` 基于 `std::deque` + `std::mutex` + 两个条件变量（非空/非满），容量上限 `MaxTaskCount`（100），`take()` 带 1 秒超时返回 `OK`/`QUEUE_FULL`/`QUEUE_STOPPED`。

## 用法

```cpp
#include "FixedThreadPool.hpp"
using namespace shanchuan;

FixedThreadPool pool(4);                       // 4 个工作线程
pool.add_task([]{ /* 异步执行 */ });

auto fut = pool.submit([](int x){ return x * 2; }, 21);
int r = fut.get();                             // 阻塞直到结果就绪 -> 42

pool.stop();
```
