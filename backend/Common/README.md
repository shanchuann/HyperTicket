# Common 公共模块

跨进程共享的公共代码。提供统一配置加载 `AppConfig`、协议定义、错误码等，供 [Server](../Server/README.md)、[Admin](../Admin/README.md) 和 [Client](../Client/README.md) 使用。

## AppConfig 配置加载

```cpp
std::string error;
hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json", &error);
if (!error.empty()) {
    LOG_FATAL << "Config load failed: " << error;
    return 1;
}
```

从 JSON 配置文件解析出强类型配置结构。DB 相关项支持被环境变量与同目录 `.env` 覆盖。

### 覆盖优先级（DB 项）

```
真实进程环境变量  >  配置文件同目录下的 .env  >  config.json 中的值
```

可覆盖的环境变量键：`DB_HOST`、`DB_PORT`、`DB_USER`、`DB_PASSWORD`、`DB_NAME`（见仓库根 `.env.example`）。
这样可以把数据库口令等敏感信息放在 `.env`（不入库）或部署环境里，而不写进 `config.json`。

### 配置结构与默认值

| 分区 | 键 | 默认值 | 说明 |
|------|----|--------|------|
| `db` | `host` | `127.0.0.1` | 数据库地址 |
| | `port` | `3306` | |
| | `user` | `root` | |
| | `password` | `""` | 建议用 `.env` / 环境变量覆盖 |
| | `name` | `hyperticket` | 库名 |
| | `pool_size` | `8` | 连接池大小 |
| `server` | `ip` | `0.0.0.0` | 监听地址 |
| | `port` | `7000` | 监听端口 |
| | `io_threads` | `4` | IO（Reactor）线程数 |
| | `worker_threads` | `8` | 业务工作线程数 |
| | `max_connections` | `1000` | 全局最大并发连接 |
| | `max_requests_per_sec` | `20` | 单连接每秒最大请求数 |
| `log` | `basename` | `hyperticket` | 日志文件名前缀 |
| | `roll_size` | `16 MiB` | 滚动大小 |
| | `flush_interval` | `3` | 冲刷间隔（秒） |
| | `level` | `INFO` | 日志等级 |
| `schedule` | `stats_interval_ms` | `60000` | 统计/会话清理周期 |
| | `ticket_status_interval_ms` | `3600000` | 票务状态巡检周期 |
| `redis` | `host` | `127.0.0.1` | Redis 地址 |
| | `port` | `6379` | Redis 端口 |
| | `pool_size` | `10` | Redis 连接池大小 |
| | `session_ttl_minutes` | `30` | Session 过期时间（分钟） |
| | `enabled` | `false` | 是否启用 Redis Session |
| `metrics` | `port` | `8080` | Metrics 暴露端口 |
| | `enabled` | `false` | 是否启用 Metrics |

配置文件示例见仓库根目录 `config.example.json`。

## 新增配置项

### Redis 配置（v1.1+）

支持将 Session 持久化到 Redis，实现多实例水平扩展。

**配置示例**：
```json
{
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "pool_size": 10,
    "session_ttl_minutes": 30,
    "enabled": false
  }
}
```

**说明**：
- `enabled: false` - 默认使用内存 SessionManager
- `enabled: true` - 使用 Redis SessionManager（需要安装 hiredis）

详见：[docs/REDIS_SESSION_GUIDE.md](../docs/REDIS_SESSION_GUIDE.md)

### Metrics 配置（v1.1+）

支持暴露 Prometheus 格式的监控指标。

**配置示例**：
```json
{
  "metrics": {
    "port": 8080,
    "enabled": false
  }
}
```

**说明**：
- `enabled: false` - 默认不启用 Metrics
- `enabled: true` - 在指定端口暴露 `/metrics` 端点

详见：[docs/PROMETHEUS_METRICS_GUIDE.md](../docs/PROMETHEUS_METRICS_GUIDE.md)

## Protocol 协议定义

**文件**：`include/Protocol.hpp`

提供 JSON 序列化/反序列化辅助函数。

**使用示例**：
```cpp
// 序列化为 JSON 行（带 \n）
Json::Value resp;
resp["code"] = 0;
resp["msg"] = "success";
std::string line = hyperticket::toJsonLine(resp);

// 解析 JSON
Json::Value req;
if (hyperticket::parseJson(line, req)) {
    int type = req["type"].asInt();
    // ...
}
```

## Errors 错误码定义

**文件**：`include/Errors.hpp`

统一错误码定义和错误响应构造。

**使用示例**：
```cpp
// 返回标准错误响应
Json::Value resp = hyperticket::makeError(hyperticket::err::kRateLimited);
// {"code": -1, "msg": "RATE_LIMITED"}

// 返回自定义错误
Json::Value resp2 = hyperticket::makeError("CUSTOM_ERROR", "详细错误信息");
// {"code": -1, "msg": "CUSTOM_ERROR", "detail": "详细错误信息"}
```

## 源码

- `include/AppConfig.hpp`：各配置 struct 与 `AppConfig::Load`
- `src/AppConfig.cpp`：JSON 解析、`.env` 加载与环境变量覆盖逻辑
- `include/Protocol.hpp`：协议辅助函数
- `include/Errors.hpp`：错误码定义

## 最佳实践

1. **配置管理**
   - 使用 `config.example.json` 作为模板
   - 不要将 `config.json` 和 `.env` 提交到版本库
   - 生产环境使用环境变量或 `.env` 管理敏感信息

2. **环境隔离**
   - 开发环境：`config.dev.json`
   - 测试环境：`config.test.json`
   - 生产环境：`config.prod.json` 或环境变量

3. **安全性**
   - 数据库密码使用 `.env` 或环境变量
   - 生产环境使用强密码
   - 定期轮换密钥

## 相关文档

- [../README.md](../README.md) - 项目概述
- [../Server/README.md](../Server/README.md) - 服务端说明
- [../Admin/README.md](../Admin/README.md) - 管理端说明
- [../docs/REDIS_SESSION_GUIDE.md](../docs/REDIS_SESSION_GUIDE.md) - Redis Session 集成
- [../docs/PROMETHEUS_METRICS_GUIDE.md](../docs/PROMETHEUS_METRICS_GUIDE.md) - Metrics 集成
