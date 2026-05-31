# SqlConnPool MySQL 连接池

> 固定大小的 MySQL 连接池，单例模式，C++17，命名空间 `shanchuan`。
> 头文件：`ConnectionPool.hpp`、`Semaphore.hpp`；编译单元：`ConnectionPool.cpp`。
> 依赖：`libmysqlclient`（MySQL C API）。

`init()` 时一次性预建好 `maxconn` 个连接，`GetConnection()` 取连接（用信号量阻塞等待空闲），`ReleaseConnection()` 归还。配套 `ConnectionGuard` 提供 RAII 作用域借还。

HyperTicket 服务端在启动时初始化连接池，业务里统一用 `ConnectionGuard` 借连接，避免每次请求重连数据库。

## 公共 API

```cpp
// 获取单例（静态局部变量，线程安全）
static ConnectionPool *GetInstance();

// 初始化：预建 maxconn 个连接，须在使用前调用一次
void init(const std::string &url, const std::string &user,
          const std::string &password, const std::string &databasename,
          int port, int maxconn, int close_log);

MYSQL *GetConnection();              // 取连接，无空闲时阻塞等待
bool   ReleaseConnection(MYSQL *);   // 归还连接并唤醒等待者
int    GetFreeConn() const;          // 当前空闲连接数
void   DestroyPool();                // 关闭并清空所有连接
```

内部用 `std::list<MYSQL*>` 存连接、`Semaphore`（C++20 前的自实现替代）记可用数、`std::mutex` 保护链表。`ConnectionPool` 与 `ConnectionGuard` 均禁止拷贝。

## 用法

```cpp
#include "ConnectionPool.hpp"
using namespace shanchuan;

ConnectionPool *pool = ConnectionPool::GetInstance();
pool->init("127.0.0.1", "root", "password", "hyperticket", 3306, 8, 0);

// 推荐：RAII 自动借还
{
    MYSQL *conn = nullptr;
    ConnectionGuard guard(&conn, pool);
    // 使用 conn ...
}   // 离开作用域自动归还

// 或手动借还
MYSQL *c = pool->GetConnection();
// ...
pool->ReleaseConnection(c);
```
