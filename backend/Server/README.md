# HyperTicket C++ 服务端

服务端基于 C++17，使用 epoll Reactor 网络层、固定业务线程池、MySQL 预处理语句、Redis Session/缓存/Streams 和定时任务。默认监听 TCP `127.0.0.1:7000`，请求和响应均为以换行分隔的 JSON。

## 请求链路

```text
TcpServer -> 换行拆包 -> JSON 校验 -> 连接限流
          -> TicketService 协议分发 -> Repository / Redis
          -> JSON 响应
```

- Inet：one loop per thread 的网络层。
- FixedThreadPool：隔离业务处理与 IO 事件循环。
- SqlConnPool / MysqlStmt：连接池和参数化 SQL。
- RedisSessionManager：带 TTL 的共享 Session 和用户 token 反向索引。
- RedisStockCache / RedisOrderQueue：库存预扣与 Redis Streams 异步订单。
- ScheduledThreadPool：订单过期、模拟支付结算、退款重试、开售提醒和统计任务。

## 当前领域模型

v10 目录以活动和场次为核心：

```text
events -> venues -> halls -> event_sessions -> ticket_tiers / seats
                                              -> reservations -> payments / refunds
```

兼容 `tickets` 表仍用于部分既有协议，但前端目录、场次库存和管理流程以 v10 表为准。

## 协议分组

协议常量以 [Protocol.hpp](../Common/include/Protocol.hpp) 为唯一事实来源。主要分组如下：

| 范围 | 能力 |
|---|---|
| `1-7` | 登录、注册、退出、浏览、异步下单、订单、取消 |
| `8-18` | 管理、删除订单、座位和验票 |
| `19-25` | 详情、支付、收藏、热门、支付查询、订单查询 |
| `26-35` | 验证码、密码重置、联系方式验证、安全状态 |
| `36-42` | 个人资料、观演人、浏览历史、开售提醒 |
| `43-46` | v10 活动目录、场次和管理员提醒审计 |

`ORDER(5)` 接收后返回 `QUEUED` 和 `request_id`，客户端使用 `ORDER_QUERY(25)` 轮询到 `PENDING` 或 `FAILED`。支付请求使用最小货币单位、Provider 和 8-64 位客户端幂等键。

## 认证与验证

- Redis Session 具有 TTL，并维护 `user_id -> tokens` 反向索引。
- 退出、修改密码和密码重置会按场景撤销 Session。
- 登录失败按账号、IP 和设备维度限制，并支持临时锁定与自动解锁。
- 管理员 Session 使用 Redis TTL；默认密码未修改时限制敏感操作。
- Verification Provider 统一邮件、开发收件箱和模拟短信的验证码发送。
- 验证码具有 TTL、冷却、每日上限、错误次数上限、单次使用和审计。

## 订单与支付

- Redis Lua 原子预扣库存并向 Stream 写入请求。
- 消费者事务写入订单、座位和观演人关联；失败时执行库存补偿。
- 超时和取消同时释放 MySQL 座位与 Redis 库存。
- 默认只启用 `MOCK` 支付；`ALIPAY`、`WECHAT` 是显式开关控制的模拟占位 Provider。
- 当前支付状态只能由模拟 Provider 可信结果推进，客户端声明成功不会确认订单。

## 配置

复制 `config.example.json` 与 `.env.example`。覆盖优先级为：

```text
config.json < .env < 进程环境变量
```

主要配置段：`server`、`db`、`redis`、`payment`、`auth`、`verification`、`order_queue`、`metrics` 和 `schedule`。真实密钥只应放在未跟踪的 `.env` 或外部密钥系统中。

## 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./scripts/run-payment-e2e.sh
./bin/ser
```

CTest 当前包含 11 个单元/集成测试；Redis Session、库存和订单队列测试需要可访问的 Redis。GitHub Actions 使用 `redis:7-alpine` 服务容器。

## 相关文档

- [异步下单架构](../../docs/ASYNC_ORDER_ARCHITECTURE.md)
- [认证安全](../../docs/AUTH_SECURITY_PHASE1.md)
- [验证码与账号生命周期](../../docs/AUTH_VERIFICATION.md)
- [支付 Provider](../../docs/PAYMENT_PROVIDERS.md)
- [v10 目录运行手册](../../docs/catalog-v10-runbook.md)
- [后端差距分析](../../docs/BACKEND_GAP_ANALYSIS.md)
