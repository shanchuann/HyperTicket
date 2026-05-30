# Server 服务端（ser）

HyperTicket 的核心服务进程，负责高并发请求处理、数据库事务与安全控制。

## 技术栈

- **网络**：自研 [Inet](../Inet/README.md) 库（基于 epoll 的主从 Reactor，one loop per thread），**非 libevent**。
- **并发**：`io_threads` 个 IO 线程跑事件循环 + [FixedThreadPool](../FixedThreadPool/README.md) 工作线程池处理业务。
- **数据库**：MySQL 8.0，经 [SqlConnPool](../SqlConnPool/README.md) 连接池借还连接。
- **日志**：[ChronoLite](../ChronoLite/README.md) 异步日志。
- **定时任务**：[ScheduledThreadPool](../ScheduledThreadPool/README.md)。
- **序列化**：jsoncpp，请求/响应为换行分隔的 JSON 行（`\n` 分包）。
- C++17，命名空间业务侧 `hyperticket`、库侧 `shanchuan`。

## 请求处理流程

1. IO 线程在 `MessageCallback` 中按 `\n` 拆出整行 JSON；
2. 先过该连接的**令牌桶限流**，超限直接回 `RATE_LIMITED`；
3. 解析 JSON，失败回 `JSON_PARSE`；
4. 把请求投递给工作线程池，由 `TicketService::handleRequest` 按 `type` 分发，处理完写回响应。

## 请求类型（`OP_TYPE`，字段 `type`）

| type | 操作 | 说明 | 需 token |
|------|------|------|:--------:|
| 1 | LOGIN | 登录，校验后签发 token | |
| 2 | REGISTER | 注册 | |
| 3 | EXIT | 退出 | |
| 4 | VIEW | 查看在售票务（status=1） | |
| 5 | ORDER | 预定票务 | ✅ |
| 6 | VIEW_MY | 查看我的预定 | ✅ |
| 7 | CANCEL | 取消预定 | ✅ |

请求 JSON 关键字段：`type`、`usertel`、`passward`、`username`、`token`、`index`。
响应：`{"status":"OK", ...}` 或 `{"status":"ERR","reason":"..."}`。

## 安全设计

**会话 token 鉴权**（`SessionManager`）
- 登录成功后签发 32 字节随机 token（64 位 hex，优先取自 `/dev/urandom`，回退 `random_device`）。
- token → `{tel, userId, expireMs}` 存于内存哈希表，加锁，worker 线程并发安全。
- TTL 默认 30 分钟，每次 `resolve()` **滑动续期**；定时任务每 60 秒清理过期 token。
- ORDER/VIEW_MY/CANCEL 一律凭 token 反查用户身份，**服务端不信任客户端自报的 tel**，杜绝越权下单/查询/取消。

**SQL 注入防护**（`MysqlStmt`）
- 含外部输入的查询统一走 `mysql_stmt_*` 预处理语句参数化绑定（`bindString`/`bindInt`）。
- 例：登录、注册、查询本人预定、取消时的归属校验、审计写入均为参数化。

**连接与频率限流**
- 全局并发连接数上限 `max_connections`（默认 1000），原子计数，超限直接 `forceClose`。
- 每条连接独立令牌桶 `RateLimiter`，容量 = `max_requests_per_sec`（默认 20/秒），存于 `TcpConnection` 的 context。

**防超卖**
- ORDER/CANCEL 在事务内对票务行 `SELECT ... FOR UPDATE` 加行锁，再扣减/回补库存并写 `reservation_audit` 审计。

## 配置

读取工作目录下的 `config.json`（见仓库根 `config.example.json`），可被 `.env` 与进程环境变量覆盖，详见 [Common/AppConfig](../Common/README.md)。关键项：

- `server.ip` / `server.port`（默认 `0.0.0.0:7000`）
- `server.io_threads` / `server.worker_threads`
- `server.max_connections` / `server.max_requests_per_sec`
- `db.*`、`db.pool_size`、`log.*`、`schedule.*`

启动时会确保目标数据库可用（不自动建表，表结构需先手动执行 `db/init.sql`）。

## 构建与运行

```bash
# 在仓库根目录
cmake -S . -B build && cmake --build build -j
./bin/ser           # 需先准备好 config.json 与已初始化的数据库
```

## 源码

- `include/ser.hpp`：`OP_TYPE` 等定义
- `include/SessionManager.hpp`：会话 token 管理
- `include/MysqlStmt.hpp`：预处理语句 RAII 封装
- `src/ser.cpp`：`TicketService` 业务逻辑 + `main` 装配
