# ScheduledThreadPool 定时任务执行器

> 基于 Linux `timerfd` + `epoll` 的定时任务执行器，C++17，命名空间 `shanchuan` / `shanchuan::scheduled`。
> 头文件：`ScheduledThreadPool.hpp`、`Timer.hpp`、`TimerQueue.hpp`、`Notcopy.hpp`；编译单元：`Timer.cpp`、`TimerQueue.cpp`。

支持一次性、延时、周期三种调度。内部 `TimerQueue` 在构造时启动一个工作线程，用 `epoll_wait` 监听所有 `timerfd`，到期即回调；周期任务自动重新装填，一次性任务触发后移除。

HyperTicket 服务端用它跑后台周期任务：统计日志、过期票务下架、过期会话 token 清理。

## 公共 API

```cpp
ScheduledThreadPool();

// 在绝对时间点执行一次
scheduled::timerId addRunAt(Timestamp time, scheduled::timerCallBack cb);

// 延时 delay 毫秒后执行一次
scheduled::timerId addRunAfter(size_t delay, scheduled::timerCallBack cb);

// 每隔 interval 毫秒周期执行
scheduled::timerId addRunEvery(size_t interval, scheduled::timerCallBack cb);

// 按 ID 取消定时器
void Cancel(scheduled::timerId timerid);
```

`timerId` 为 `<int fd, Timer*>`；`Timer` 基于 `timerfd_create(CLOCK_MONOTONIC)`，`TimerQueue` 基于 `epoll_create1`，`Timer`/`TimerQueue` 继承 `Notcopy` 禁止拷贝。

## 用法

```cpp
#include "ScheduledThreadPool.hpp"
using namespace shanchuan;

ScheduledThreadPool scheduler;
scheduler.addRunAfter(1000, []{ /* 1 秒后执行一次 */ });
auto id = scheduler.addRunEvery(500, []{ /* 每 500ms 执行 */ });
// ...
scheduler.Cancel(id);
```
