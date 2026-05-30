# Common 公共模块

跨进程共享的公共代码。当前提供统一配置加载 `AppConfig`，供 [Server](../Server/README.md) 与 [Admin](../Admin/README.md) 使用。

## AppConfig 配置加载

```cpp
hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json", &error);
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
| | `password` | `zbk` | 建议用 `.env` / 环境变量覆盖 |
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

配置文件示例见仓库根目录 `config.example.json`。

## 源码

- `include/AppConfig.hpp`：各配置 struct 与 `AppConfig::Load`
- `src/AppConfig.cpp`：JSON 解析、`.env` 加载与环境变量覆盖逻辑
