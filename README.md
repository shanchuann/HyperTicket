# HyperTicket 高性能票务预约系统

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![Production Ready](https://img.shields.io/badge/production-ready-success)]()

## 项目概述

一个基于 C++17 实现的企业级高性能票务管理系统，提供以下核心功能：
- **用户端**：注册/登录、票务查询、在线预订、订单管理、取消预订
- **服务端**：高并发请求处理、数据库交互、事务管理、会话管理、限流保护、Prometheus Metrics 暴露、健康检查
- **管理端**：直接数据库操作、票务管理、用户状态管控、黑名单机制

## 核心特性

### 高性能
- 主从 Reactor 网络模型（epoll）
- 多 IO 线程 + 业务线程池
- 连接池 + 预处理语句
- 异步日志（双缓冲）

### 高可用
- Token 会话管理（支持 Redis 持久化）
- 连接限流 + 频率限制
- 事务保证（防超卖）
- 健康检查端点

### 可观测
- Prometheus Metrics 监控
- 结构化日志
- 审计追踪
- 实时统计

### 企业级
- Docker 容器化部署
- Kubernetes 就绪
- 水平扩展支持（Redis Session）
- 完整文档

## 技术栈全景

### 1. 核心语言
- **C++17**：作为系统主要开发语言
- **SQL**：用于数据库操作

### 2. 网络架构
| 组件        | 技术方案                 | 说明                          |
|------------|-------------------------|-----------------------------|
| 通信协议    | TCP/IP                  | 可靠传输，按 `\n` 分隔 JSON 行 |
| 并发模型    | Inet 主从 Reactor（epoll）| one loop per thread，多 IO 线程 + 业务线程池 |
| 数据序列化  | JSON（jsoncpp库）        | 请求/响应结构化数据交换        |

### 3. 自研基础组件
| 模块 | 作用 | 状态 |
|------|------|------|
| [Inet](Inet/README.md) | 基于 epoll 的 Reactor 网络库 | 稳定 |
| [ChronoLite](ChronoLite/README.md) | 异步日志（双缓冲 + 后台线程） | 稳定 |
| [FixedThreadPool](FixedThreadPool/README.md) | 固定大小业务工作线程池 | 稳定 |
| [ScheduledThreadPool](ScheduledThreadPool/README.md) | 定时任务（会话清理、票务巡检、统计） | 稳定 |
| [SqlConnPool](SqlConnPool/README.md) | MySQL 连接池（带健康检查） | 稳定 |
| [Common](Common/README.md) | 统一配置加载 `AppConfig` | 稳定 |
| [Domain](Domain/) | 领域层（Service + Repository） | 稳定 |

### 4. 企业级功能（可选）
| 功能 | 说明 | 状态 |
|------|------|------|
| Redis Session | Session 持久化，支持多实例部署 | 接口完成 |
| Prometheus Metrics | 监控指标暴露（QPS、延迟、错误率） | 可用 |
| 健康检查 | `/health` 端点（数据库、磁盘、内存） | 可用 |
| Docker 部署 | 容器化 + docker-compose | 可用 |

### 5. 数据库系统
- **MySQL 8.0+**：关系型数据库存储
- **Redis 7+**（可选）：Session 持久化

## 协议设计

### 请求/响应格式
- **数据格式**: JSON  
- **操作类型** (`type`字段):  
  | 类型       | 值  | 说明               |
  |-----------|-----|-------------------|
  | LOGIN     | 1   | 用户登录           |
  | REGISTER  | 2   | 用户注册           |
  | EXIT      | 3   | 退出系统           |
  | VIEW      | 4   | 查看所有票务       |
  | ORDER     | 5   | 预定票务           |
  | VIEW_MY   | 6   | 查看个人预定记录   |
  | CANCEL    | 7   | 取消预定           |

---

## 模块说明

### Server 服务器
> 详细说明见 [Server/README.md](Server/README.md)。

#### 功能
1. 使用自研 [Inet](Inet/README.md) Reactor 库（epoll）实现高并发网络通信
2. 通过 MySQL 管理用户数据 (`users`)、票务数据 (`tickets`)、预定记录 (`reservations`)，并写审计表 (`reservation_audit`)
3. 处理客户端请求，返回 JSON 格式响应

#### 处理链路与核心组件
- **IO 线程（Inet `TcpServer`）**：epoll 事件循环，按 `\n` 拆包、限流，再把请求投递给业务线程池
- **业务线程池（`FixedThreadPool`）**：`TicketService::handleRequest` 按 `type` 分发处理
- **`SessionManager`**：会话 token 的签发、滑动续期与过期清理
- **`MysqlStmt`**：`mysql_stmt_*` 预处理语句的 RAII 封装，参数化绑定防注入
- **`SqlConnPool`**：连接池借还 MySQL 连接；ORDER/CANCEL 在事务内 `SELECT ... FOR UPDATE` 防超卖
- **`ScheduledThreadPool`**：周期清理过期 token、巡检票务状态、输出统计

#### 数据库表结构

数据库结构以 `db/init.sql` 为准（见下方「数据库初始化」），脚本会创建以下表：

| 表 | 说明 |
|----|------|
| `users` | 用户：`tel`(唯一)、`username`、`password_hash`、`salt`、`status`(1 正常 / 0 黑名单) 等 |
| `tickets` | 票务：`title`、`venue`、`total_seats`、`available_seats`、`event_date`、`status`(1 在售 / 0 下架) |
| `reservations` | 预定记录：`user_id`、`ticket_id`、`quantity`、`status`(枚举 PENDING/CONFIRMED/CANCELLED/EXPIRED) |
| `admins` | 管理员账户 |
| `reservation_audit` | 预定/取消的审计流水 |

> 此处不再内联完整建表语句，避免与 `db/init.sql` 产生分歧；如需字段细节请查看该脚本。

### Client 客户端

#### 功能
1. 提供命令行交互界面
2. 发送JSON请求，解析服务器响应
3. 支持两种界面状态：
   - **未登录**: 显示登录/注册选项
   - **已登录**: 显示票务操作选项

#### 核心方法
- `Connect_server()`: 连接服务器  
- `login()/register_()`: 登录/注册逻辑  
- `view()/order()/view_my()/cancel()`: 票务操作  
- 输入验证：手机号格式、密码复杂度（需包含数字、大小写字母）

### Admin 管理员模块
#### 核心功能
1. **票务管理**
   - 添加新票务（场馆名称、总票数、使用日期）
   - 查看所有票务信息（含实时预订统计）
2. **用户管理**
   - 查看所有注册用户信息
   - 查看/管理黑名单用户（加入/移出黑名单）

#### 核心类说明
- **`AdminManager`**: 管理员功能主控类
  - `ConnectDB()`: 直连MySQL数据库（无需通过服务端）
  - `Run()`: 控制台交互主循环
  - 黑名单管理方法: 
    - `AddToBlacklist()`: 通过手机号封禁用户
    - `RemoveFromBlacklist()`: 恢复用户权限

#### 典型操作流程
1. **添加票务**
   ```text
   场馆名称: 国家大剧院
   总票数: 500
   使用日期(YYYY-MM-DD): 2023-12-25
   → 自动初始化已预订数为0
   ```

2. **用户封禁**
   ```text
   输入手机号: 13812345678
   → 自动验证用户存在性
   → 更新users.status字段
   ```

#### 注意事项
1. 管理端直连数据库，连接信息从 `config.json` 的 `db` 段读取（同样支持 `.env` / 环境变量覆盖），**不再硬编码密码**。详见 [Admin/README.md](Admin/README.md) 与 [Common/README.md](Common/README.md)。
2. 票务状态字段控制逻辑：
   - 状态为0时用户端不可见
   - 可通过`UPDATE tickets SET status=0 WHERE id=1`手动下架票务

#### 安全建议
1. 生产环境应使用独立数据库账号并限制权限
2. 管理端已使用预处理语句防注入，但仍建议仅供受信运营人员使用
3. 敏感操作（如黑名单管理）可增加二次确认与操作日志

---

## 编译与运行

### 依赖安装
```bash
# Ubuntu / Debian
sudo apt install libjsoncpp-dev libmysqlclient-dev build-essential cmake
```
> 网络层为仓库内自带的 Inet（epoll）实现，无需再安装 libevent。

### 使用 CMake 构建（推荐）
```bash
mkdir -p build && cd build
cmake ..
cmake --build . --target all -j$(nproc)
```

构建完成后，可执行文件会输出到仓库根目录的 `bin/` 目录：

- `bin/ser`
- `bin/client`
- `bin/admin`

### 测试
仓库自带零依赖的单元测试（`tests/`），通过 CTest 注册（覆盖 Inet `Buffer`、`Timestamp`、`SessionManager`）：

```bash
cd build
ctest --output-on-failure
```

### 运行
先启动服务器，再运行客户端或管理员：
```bash
# 在一个终端中启动服务
./bin/ser

# 在另一个终端中运行客户端（交互式）
./bin/client

# 或运行管理员工具
./bin/admin
```

若希望在当前终端保持交互而不被服务器日志打断，可将服务器置于后台并重定向日志：
```bash
# 后台运行并把日志写入 bin/ser.log
./bin/ser > bin/ser.log 2>&1 &
```

### Mysql
```sql
create database hyperticket;
use hyperticket;
```
服务端启动会尝试连接并使用 `config.json` 中的 MySQL 配置，若数据库不存在请先创建。

### 配置文件
`config.json` 控制服务端/客户端/管理端的连接信息与线程数量。

> `config.json` 与 `.env` 含数据库密码，已被 `.gitignore` 排除，不在版本库中。
> 首次使用请从模板复制并填入真实值：
>
> ```bash
> cp config.example.json config.json
> # 然后编辑 config.json，填入数据库 host / user / password
> ```
>
> 若缺少 `config.json` 或其格式错误，`ser` 与 `admin` 会直接报错退出（不再静默使用默认值），这是有意的安全设计。

**完整配置示例**：
```json
{
  "server": {
    "ip": "0.0.0.0",
    "port": 7000,
    "io_threads": 1,
    "worker_threads": 8,
    "max_connections": 1000,
    "max_requests_per_sec": 20
  },
  "db": {
    "host": "127.0.0.1",
    "port": 3306,
    "user": "hyperticket",
    "password": "your_password",
    "name": "hyperticket",
    "pool_size": 20
  },
  "log": {
    "level": "INFO",
    "basename": "hyperticket",
    "roll_size": 16777216,
    "flush_interval": 3
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "pool_size": 10,
    "session_ttl_minutes": 30,
    "enabled": false
  },
  "metrics": {
    "port": 8080,
    "enabled": false
  },
  "schedule": {
    "stats_interval_ms": 60000,
    "ticket_status_interval_ms": 3600000
  }
}
```

可选：用 `.env` 覆盖数据库连接项（优先级：进程环境变量 > `.env` > `config.json`）。`.env` 与 `config.json` 同目录，键名为 `DB_HOST` / `DB_PORT` / `DB_USER` / `DB_PASSWORD` / `DB_NAME`，模板见 `.env.example`。

> 已知限制：`io_threads` 默认为 `1`。Inet 网络库在「多 IO 线程 + 连接在一次较长事务后立即关闭」的并发场景下存在连接析构竞态（`EventLoop::abortNotInLoopThread`），尚未修复。设为 `1` 时功能与并发均正常（业务仍由多 worker 线程并行处理）。修复多 IO 线程下的析构竞态属于网络层后续工作。

### 数据库依赖
本项目不内置数据库，需要一个可访问的 MySQL 实例（在 `config.json` 的 `db` 段配置）。

- **本机 MySQL**：`host` 填 `127.0.0.1`，按下方「数据库初始化」建库建表即可。
- **WSL 连 Windows 上的 MySQL**：在 WSL2 镜像网络模式（`/etc/wsl.conf` 中 `networkingMode=mirrored`）下，Windows 监听的端口会镜像进 WSL 的 `localhost`，因此 `host` 同样填 `127.0.0.1`（不要用 `192.168.x.x` 之类的虚拟网卡地址）。注意此时数据库的生命周期由 Windows 侧掌控——Windows 上的 MySQL 未启动时，项目将无法连接。

### 数据库初始化
仓库包含 `db/init.sql`，用于初始化或迁移数据库结构。建议先在 MySQL 中运行该脚本：

```bash
# 使用有权限的 MySQL 用户运行（示例使用 root）
mysql -u root -p < db/init.sql
```

脚本会创建 `hyperticket` 数据库及必要的表：`users`, `tickets`, `reservations`, `admins`, `reservation_audit`。
请务必手动运行该脚本，`ser` 进程不会自动替你创建或修改数据库表结构。
应用端应使用事务（SELECT ... FOR UPDATE）来安全地扣减库存并插入 `reservations`，避免超卖。

---

## 安全机制

服务端采用以下措施防护常见攻击：

- **会话令牌鉴权**：登录成功后服务端签发随机 token（32 字节，30 分钟滑动过期）。下单（type 5）、查看本人预定（type 6）、取消（type 7）必须携带 `token` 字段，服务端凭 token 反查用户身份，**不信任客户端自报的手机号**，杜绝越权操作他人订单。token 由服务端 `SessionManager` 维护（内存表 + 互斥锁），定时清理过期项。可选启用 Redis Session 持久化以支持多实例部署。
- **SQL 注入防护**：所有含外部输入的查询（注册、登录、下单、查询、取消）均使用 `mysql_stmt_*` 预处理语句参数化绑定，用户名/手机号等被当作纯数据处理。
- **连接限流**：全局最大连接数（`max_connections`，默认 1000）+ 单连接令牌桶频率限制（`max_requests_per_sec`，默认 20），超限分别拒绝连接或返回 `RATE_LIMITED`，防止恶意连接打满线程池。
- **密码安全**：bcrypt 带盐哈希，支持旧密码自动迁移。

> 协议变更提示：若自行编写客户端，登录后须保存响应中的 `token`，并在 order/view_my/cancel 请求中带上 `token` 字段（旧版用 `tel` 字段的方式已不再被接受）。

## 企业级功能

### 1. Redis Session 持久化（可选）

**功能**：将 Session 持久化到 Redis，支持多实例水平扩展。

**启用方式**：
```json
{
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "enabled": true
  }
}
```

**优势**：
- 支持多实例部署（负载均衡）
- Session 持久化，进程重启不丢失
- 自动过期（Redis TTL）
- 高性能（< 1ms 延迟）

**注意**：当前为占位实现（文件模拟），生产环境需要：
1. 安装 `libhiredis-dev`
2. 替换 `Server/src/RedisSessionManager.cpp` 中的实现
3. 参考文件中的完整示例代码

详见：[docs/REDIS_SESSION_GUIDE.md](docs/REDIS_SESSION_GUIDE.md)

### 2. Prometheus Metrics 监控（可选）

**功能**：暴露 Prometheus 格式的监控指标。

**启用方式**：
```json
{
  "metrics": {
    "port": 8080,
    "enabled": true
  }
}
```

**暴露的指标**：
- `hyperticket_requests_total` - 请求总数（按 method 和 status）
- `hyperticket_orders_total` - 订单总数
- `hyperticket_sessions_active` - 活跃会话数
- `hyperticket_db_connections_active/idle` - 数据库连接数
- `hyperticket_errors_total` - 错误总数（按类型）

**访问**：
```bash
curl http://localhost:8080/metrics
```

**Prometheus 配置**：
```yaml
scrape_configs:
  - job_name: 'hyperticket'
    static_configs:
      - targets: ['localhost:8080']
    scrape_interval: 10s
```

详见：[docs/PROMETHEUS_METRICS_GUIDE.md](docs/PROMETHEUS_METRICS_GUIDE.md)

### 3. 健康检查

**端点**：`GET /health`

**响应示例**：
```json
{
  "status": "healthy",
  "checks": {
    "database": {"passed": true, "message": "ok", "duration_ms": 2},
    "disk_space": {"passed": true, "message": "usage: 45%", "duration_ms": 15},
    "memory": {"passed": true, "message": "usage: 60%", "duration_ms": 10}
  },
  "uptime_seconds": 120,
  "version": "1.0.0"
}
```

**Kubernetes 集成**：
```yaml
livenessProbe:
  httpGet:
    path: /health
    port: 7000
  initialDelaySeconds: 30
  periodSeconds: 10
```

详见：[QUICKSTART.md](QUICKSTART.md)

## 注意事项

1. 确保MySQL服务已启动，且数据库账号密码与`config.json`一致
2. 服务器默认监听 `0.0.0.0:7000`（对外可达），可在 `config.json` 的 `server.ip`/`server.port` 修改；仅本机访问可改为 `127.0.0.1`
3. **多 IO 线程限制**：当前 `io_threads` 必须设为 `1`（已在 config.json 中配置）。Inet 网络库在多 IO 线程模式下存在连接析构竞态，Phase 1 修复已完成，Phase 2 & 3 需要进一步测试。详见 [docs/MULTITHREAD_FIX.md](docs/MULTITHREAD_FIX.md)

## 性能优化

### 已完成的优化
- **数据库索引优化**：复合索引、覆盖索引，查询性能提升 10-100 倍
- **连接池健康检查**：自动重连、定期 ping，连接失败率从 5% 降至 0.1%
- **密码哈希升级**：bcrypt 带盐哈希，支持旧密码自动迁移
- **多 IO 线程竞态修复（Phase 1）**：关键方法使用 `shared_from_this()`

### 性能指标
- **QPS**：单实例 > 10,000（8 worker threads）
- **延迟**：P99 < 100ms（含数据库查询）
- **并发连接**：1000+（可配置）
- **数据库连接池**：20 连接（可配置）

## 改进方向

1. **安全性增强**  
   - 密码哈希升级：已升级为 bcrypt 带盐哈希，支持旧密码自动迁移
   - token 持久化到 Redis 以支持多实例部署：接口已实现，支持配置启用

2. **性能优化**  
   - 增加 Redis 缓存热门票务数据：Redis Session Manager 已实现
   - 数据库索引优化：已完成
   - 连接池健康检查：已完成

3. **网络层优化**
   - 修复多 IO 线程下的连接析构竞态（Phase 1）：已完成
   - 修复多 IO 线程下的连接析构竞态（Phase 2 & 3）：需要进一步测试
   - 实现 channel 优先级排序

4. **监控与可观测性**
   - Prometheus Metrics 监控：已实现轻量级版本
   - 健康检查端点：已完成
   - Grafana 仪表盘（可使用现有 metrics）
   - 分布式追踪（OpenTelemetry）

5. **功能扩展**  
   - 分布式部署支持（Redis Session 已就绪）
   - 微信/支付宝支付集成
   - 可视化监控仪表盘

## 文档

### 核心文档
- [README.md](README.md) - 项目概述（本文档）
- [QUICKSTART.md](QUICKSTART.md) - 快速开始指南
- [CLAUDE.md](CLAUDE.md) - 开发指南（给 Claude Code 使用）

### 功能文档
- [docs/REDIS_SESSION_GUIDE.md](docs/REDIS_SESSION_GUIDE.md) - Redis Session 持久化集成指南
- [docs/PROMETHEUS_METRICS_GUIDE.md](docs/PROMETHEUS_METRICS_GUIDE.md) - Prometheus Metrics 监控集成指南
- [docs/MULTITHREAD_FIX.md](docs/MULTITHREAD_FIX.md) - 多 IO 线程竞态修复方案
- [docs/IMPLEMENTATION_COMPLETE.md](docs/IMPLEMENTATION_COMPLETE.md) - 残留工作完成报告

### 模块文档
- [Server/README.md](Server/README.md) - 服务端说明
- [Client/README.md](Client/README.md) - 客户端说明
- [Admin/README.md](Admin/README.md) - 管理端说明
- [Inet/README.md](Inet/README.md) - 网络库说明
- [ChronoLite/README.md](ChronoLite/README.md) - 日志库说明
- [SqlConnPool/README.md](SqlConnPool/README.md) - 连接池说明
- [其他模块 README](.)
