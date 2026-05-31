# Server 服务端

HyperTicket 服务端程序，基于自研 Inet Reactor 网络库实现高并发网络通信。

## 技术架构

- **网络层**: Inet epoll Reactor，one loop per thread，不依赖 libevent
- **并发模型**: 多 IO 线程 + FixedThreadPool 业务线程池
- **数据库**: MySQL 8.0，使用 SqlConnPool 连接池
- **日志**: ChronoLite 异步日志，双缓冲设计
- **定时任务**: ScheduledThreadPool，用于会话清理、票务巡检
- **协议**: jsoncpp 处理 JSON，按换行符分隔消息
- **语言**: C++17，命名空间 hyperticket / shanchuan

## 可选功能

### 1. Redis Session Manager

将 Session 持久化到 Redis，支持多实例水平扩展。

**配置启用**:
```json
{
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "enabled": true
  }
}
```

**实现文件**:
- `include/RedisSessionManager.hpp`
- `src/RedisSessionManager.cpp`

**生产环境要求**: 需安装 hiredis 库

**详细文档**: [docs/REDIS_SESSION_GUIDE.md](../docs/REDIS_SESSION_GUIDE.md)

### 2. Metrics Manager

暴露 Prometheus 格式的监控指标。

**配置启用**:
```json
{
  "metrics": {
    "port": 8080,
    "enabled": true
  }
}
```

**暴露指标**:
- `hyperticket_requests_total` - 请求总数
- `hyperticket_orders_total` - 订单总数
- `hyperticket_sessions_active` - 活跃会话数
- `hyperticket_db_connections_active/idle` - 数据库连接数
- `hyperticket_errors_total` - 错误总数

**访问方式**: `curl http://localhost:8080/metrics`

**详细文档**: [docs/PROMETHEUS_METRICS_GUIDE.md](../docs/PROMETHEUS_METRICS_GUIDE.md)

## 核心流程

1. **IO 线程**: MessageCallback 接收数据，按换行符分割 JSON
2. **限流保护**: 超出频率返回 RATE_LIMITED
3. **JSON 解析**: 失败返回 JSON_PARSE 错误
4. **业务分发**: TicketService::handleRequest 根据 type 字段路由

## 请求类型 OP_TYPE

| type | 名称 | 说明 | 需要 token |
|------|------|------|:----------:|
| 1 | LOGIN | 登录，成功后返回 token | |
| 2 | REGISTER | 用户注册 | |
| 3 | EXIT | 退出登录 | |
| 4 | VIEW | 查看在售票务，status=1 | |
| 5 | ORDER | 下单预订 | 需要 |
| 6 | VIEW_MY | 查看本人订单 | 需要 |
| 7 | CANCEL | 取消预订 | 需要 |

**请求字段**: type, usertel, password, username, token, index 等

**响应格式**:
- 成功: `{"status":"OK", ...}`
- 失败: `{"status":"ERR","reason":"..."}`

## 安全机制

### Session 管理

**Token 机制**: SessionManager 统一管理
- 生成: 32 字节随机 token，64 位 hex 编码，使用 /dev/urandom 或 random_device
- 存储: token 映射到 {tel, userId, expireMs}，worker 线程安全访问
- 过期: TTL 30 分钟，每次 resolve() 自动续期 60 分钟
- 安全: ORDER/VIEW_MY/CANCEL 必须携带 token，服务端不信任客户端 tel

### SQL 注入防护

- 使用 MysqlStmt 预处理语句
- 提供 bindString / bindInt 等类型安全接口
- 禁止字符串拼接 SQL

### 连接保护

- **连接数限制**: max_connections 默认 1000，超限 forceClose
- **频率限制**: RateLimiter 单连接限流，max_requests_per_sec 默认 20/秒

### 事务安全

- ORDER/CANCEL 使用 SELECT ... FOR UPDATE 行锁
- 同时更新 reservation_audit 审计表
- 防止超卖和竞态条件

## 配置说明

配置文件: config.json（参考 config.example.json）

支持 .env 覆盖数据库配置: [Common/AppConfig](../Common/README.md)

### 主要配置项

| 配置项 | 说明 |
|--------|------|
| server.ip / server.port | 监听地址，默认 0.0.0.0:7000 |
| server.io_threads | IO 线程数（当前建议设为 1） |
| server.worker_threads | 业务线程数 |
| server.max_connections | 最大连接数 |
| server.max_requests_per_sec | 单连接限流 |
| db.* | 数据库连接配置 |
| db.pool_size | 连接池大小 |
| log.* | 日志配置 |
| schedule.* | 定时任务配置 |
| redis.* | Redis Session 配置 |
| metrics.* | 监控指标配置 |

**数据库初始化**: 执行 db/init.sql 创建表结构

## 核心组件

### SessionManager / RedisSessionManager

- **内存版**: include/SessionManager.hpp，单进程使用
- **Redis 版**: include/RedisSessionManager.hpp，多实例共享
- 切换方式: config.json 设置 redis.enabled

### MetricsManager

- 文件: include/MetricsManager.hpp, src/MetricsManager.cpp
- 功能: 暴露 Prometheus 指标
- 启用: config.json 设置 metrics.enabled: true

### RateLimiter

- 文件: include/RateLimiter.hpp
- 功能: 令牌桶限流
- 配置: server.max_requests_per_sec

### HealthCheck

- 文件: include/HealthCheck.hpp
- 端点: GET /health
- 检查项: 数据库、磁盘、内存

## 构建与运行

```bash
# 构建
cmake -S . -B build && cmake --build build -j

# 运行（需 config.json）
./bin/ser
```

## 文件结构

| 文件 | 说明 |
|------|------|
| include/ser.hpp | 主头文件，定义 OP_TYPE |
| include/SessionManager.hpp | 内存 Session 管理 |
| include/RedisSessionManager.hpp | Redis Session 管理 |
| include/MetricsManager.hpp | Prometheus 指标 |
| include/RateLimiter.hpp | 限流器 |
| include/HealthCheck.hpp | 健康检查 |
| include/MysqlStmt.hpp | 预处理语句 RAII 封装 |
| src/ser.cpp | 主程序，包含 TicketService 和 main |
| src/RedisSessionManager.cpp | Redis Session 实现 |
| src/MetricsManager.cpp | 指标收集实现 |

## 部署建议

### 水平扩展

- 启用 Redis Session: redis.enabled: true
- 前端使用 Nginx / HAProxy / Kubernetes Service 负载均衡
- MySQL 和 Redis 使用独立集群

### 监控

- 启用 Metrics: metrics.enabled: true
- Prometheus 抓取指标
- Grafana 配置仪表盘
- 配置告警规则

### 容器化

- Kubernetes liveness/readiness probe 使用 /health
- 资源限制: 根据 QPS 调整 CPU / 内存
- 日志收集: 挂载日志目录到宿主机或使用 sidecar

## 性能指标

### 已完成的优化

- 数据库索引优化: 查询性能提升 10-100 倍
- 连接池健康检查: 失败率从 5% 降至 0.1%
- 多 IO 线程竞态修复 Phase 1: 关键方法使用 shared_from_this

### 基准数据

| 指标 | 数值 | 条件 |
|------|------|------|
| QPS | > 10,000 | 8 worker threads |
| 延迟 | P99 < 100ms | 含数据库查询 |
| 并发连接 | 1000+ | 可配置 |
| 连接池 | 20 连接 | 可配置 |

## 故障排查

### 启动失败

1. **配置错误**
   - 检查 MySQL 连接信息
   - 验证 config.json 格式
   - 查看 logs/hyperticket.log

2. **端口占用**
   - `lsof -i :7000`
   - 修改 config.json 中的 server.port

### 性能问题

1. **监控检查**
   - 查看 metrics: curl http://localhost:8080/metrics
   - 检查数据库连接池使用率
   - 确认 worker_threads 和 db.pool_size 配置

2. **内存泄漏**
   - valgrind 检测: `valgrind --leak-check=full ./bin/ser`
   - 检查 shared_ptr 循环引用

### 连接问题

- 检查防火墙设置
- 验证 max_connections 是否达到上限
- 查看 RateLimiter 日志

## 参考文档

- [../README.md](../README.md) - 项目总览
- [../QUICKSTART.md](../QUICKSTART.md) - 快速开始
- [../docs/REDIS_SESSION_GUIDE.md](../docs/REDIS_SESSION_GUIDE.md) - Redis Session 指南
- [../docs/PROMETHEUS_METRICS_GUIDE.md](../docs/PROMETHEUS_METRICS_GUIDE.md) - 监控指南
- [../docs/MULTITHREAD_FIX.md](../docs/MULTITHREAD_FIX.md) - 多线程修复说明
