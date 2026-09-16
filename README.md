# HyperTicket 高性能票务预约系统

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)]()
[![Qt6](https://img.shields.io/badge/Qt-6-green)]()
[![React](https://img.shields.io/badge/React-18-61dafb)]()
[![Tauri](https://img.shields.io/badge/Tauri-2-ffc131)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-green)]()

基于 **C++17** 实现的企业级高性能票务管理系统，支持全平台多端访问。

- **C++ 服务器**：自研 epoll Reactor 网络库，主从多线程模型
- **CLI 客户端**：终端交互式命令行工具
- **Web 前端**：React + TypeScript，含营销落地页、用户端、管理后台
- **桌面客户端**：Qt6 QML (管理员) + Tauri 2 (用户端)
- **部署**：Docker 容器化，可选 Prometheus + Grafana 监控

---

## 快速开始

### 1. 环境准备

```bash
# 依赖安装（Ubuntu/Debian）
sudo apt install libjsoncpp-dev libmysqlclient-dev build-essential cmake
```

### 2. 编译 C++ 后端

```bash
# 构建所有 C++ 可执行文件（ser / client / admin）
mkdir -p build && cd build
cmake ..
cmake --build . --target all -j$(nproc)

# 可执行文件输出到项目根目录的 bin/
cd ..
# bin/ser       - 服务器
# bin/client    - CLI 客户端
# bin/admin     - 管理员命令行工具
```

### 3. 配置数据库

```bash
# 创建数据库和表结构
mysql -u root -p < db/init.sql

# 复制配置模板并编辑
cp config.example.json config.json
# 编辑 config.json，填入数据库密码等配置
```

### 4. 启动服务

**方式 A — 直接启动（开发/测试）**

```bash
# 终端 1: 启动服务器
./bin/ser

# 终端 2: 启动 CLI 客户端
./bin/client

# 终端 3: 启动管理员工具
./bin/admin
```

后台运行（不占用终端）：

```bash
./bin/ser > logs/ser.log 2>&1 &
```

**方式 B — Docker 容器化（生产）**

```bash
# 全栈启动（含 MySQL + Redis）
docker-compose up -d

# 带监控启动（含 Prometheus + Grafana）
docker-compose --profile monitoring up -d
```

### 5. 启动 Web 前端

```bash
# 步骤一：启动后端服务（见第 4 步）

# 步骤二：启动 WebSocket 桥接服务器
cd websocket-bridge
npm install     # 首次需要
npm start       # 监听 ws://localhost:8080

# 步骤三：启动前端开发服务器
cd ../frontend
npm install     # 首次需要
npm run dev     # 访问 http://localhost:3000
```

一键启动脚本：

```bash
./scripts/start-frontend.sh
```

### 6. 启动桌面客户端

见下方 [桌面客户端](#桌面客户端) 章节。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            客户端层                                      │
│  ┌──────────┐  ┌─────────────┐  ┌───────────┐  ┌───────────┐            │
│  │ CLI 终端  │  │ Qt6 管理端  │  │ React Web │  │ Tauri 桌面│            │
│  │ (C++ TCP) │  │ (C++ + QML)│  │(WebSocket)│  │ (Rust TCP)│            │
│  └─────┬─────┘  └──────┬─────┘  └─────┬─────┘  └─────┬─────┘            │
│        │               │               │               │                │
│        │    TCP        │    TCP        │  WebSocket    │    TCP         │
│        │    :7000      │    :7000      │  :8080        │    :7000       │
│        ▼               ▼               ▼               ▼                │
│                                  ┌──────────┐                           │
│                                  │ 桥接服务  │                           │
│                                  │ (Node.js)│                           │
│                                  └─────┬────┘                           │
└────────────────────────────────────────┼────────────────────────────────┘
                                         │ TCP :7000
                                         ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                            服务端层                                      │
│  ┌────────────────── Inet 网络库 (epoll Reactor) ─────────────────────┐  │
│  │  Acceptor → EventLoopThreadPool (IO 线程) → 拆包、限流、分发        │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                  │                                      │
│                                  ▼                                      │
│  ┌─────────── FixedThreadPool (业务线程池) ───────────────────────────┐  │
│  │  TicketService::handleRequest → Repository → MySQL                │  │
│  │  事务管理 (SELECT ... FOR UPDATE 防超卖)                           │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                  │                                      │
│                                  ▼                                      │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │  SqlConnPool (连接池) │ SessionManager (Token) │ ChronoLite (日志) │  │
│  │  ScheduledThreadPool (定时任务) │ RateLimiter (限流)               │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                             数据层                                      │
│  ┌──────────┐  ┌──────────┐                                             │
│  │  MySQL 8 │  │  Redis 7 │ (可选 Session 持久化)                        │
│  └──────────┘  └──────────┘                                             │
└─────────────────────────────────────────────────────────────────────────┘
```

### 请求处理链路

1. **IO 线程** (Inet TcpServer)：epoll 事件循环 → 按 `\n` 拆包 → JSON 解析 → RateLimiter 限流
2. **业务线程** (FixedThreadPool)：`TicketService::handleRequest` 按 `type` 分发
3. **数据库**：`SqlConnPool` 借连接 → `MysqlStmt` 预处理语句 → 事务处理
4. **响应**：JSON 序列化写回 `TcpConnection`

---

## 核心特性

### 高性能
- 主从 Reactor 网络模型（epoll），one loop per thread
- IO 线程 + FixedThreadPool 业务线程池分离
- SqlConnPool 连接池 + MysqlStmt 预处理语句
- ChronoLite 异步日志（双缓冲 + 后台线程）
- QPS > 10,000（8 worker），P99 < 100ms

### 高可用
- Token 会话管理（30 分钟滑动过期，支持 Redis 持久化）
- 令牌桶限流（连接级）+ 全局连接上限
- `SELECT ... FOR UPDATE` 事务防超卖
- 健康检查端点（数据库、磁盘、内存）

### 安全防护
- **SQL 注入防御**：所有查询使用 `mysql_stmt_*` 参数化绑定
- **会话鉴权**：服务端签发随机 32 字节 token，不信任客户端自报身份
- **密码安全**：bcrypt 带盐哈希，支持旧密码自动迁移
- **连接保护**：最大连接数 + 单连接频率限制

### 可观测性
- Prometheus Metrics：QPS、延迟、订单数、活跃会话、错误率
- 结构化异步日志 + 审计追踪表
- 定时统计输出

---

## 技术栈

### C++ 后端组件

所有后端模块位于 `backend/` 目录：

| 模块 | 说明 | 文档 |
|------|------|------|
| [Inet](backend/Inet/README.md) | 基于 epoll 的 Reactor 网络库，muduo 风格 one loop per thread | 稳定 |
| [ChronoLite](backend/ChronoLite/README.md) | 异步日志（双缓冲 + 后台线程），微秒级时间戳 | 稳定 |
| [FixedThreadPool](backend/FixedThreadPool/README.md) | 固定大小业务线程池 + 有界阻塞队列 | 稳定 |
| [ScheduledThreadPool](backend/ScheduledThreadPool/README.md) | 定时任务（会话清理、票务巡检、统计） | 稳定 |
| [SqlConnPool](backend/SqlConnPool/README.md) | MySQL 连接池（单例，信号量阻塞等待，健康检查） | 稳定 |
| [Common](backend/Common/README.md) | AppConfig 统一配置加载（config.json + .env + 环境变量） | 稳定 |
| [Domain](backend/Domain/) | 领域层 — TicketService + Repository 接口 | 稳定 |
| [Server](backend/Server/README.md) | 服务端主程序（ser），含 SessionManager、MetricsManager 等 | 稳定 |

### 前端/客户端技术栈

| 技术 | 用途 | 客户端 |
|------|------|--------|
| React 19 + TypeScript + Vite | Web 前端框架 | Web 用户端、营销页、管理后台 |
| React Router 7 | 客户端路由 | Web 前端 |
| Framer Motion + Lucide React | 动画 + 图标 | Web 前端 |
| Qt 6 + QML + Material Design 3 | 桌面 UI 框架 | Qt6 管理端 |
| Tauri 2 + Rust | 桌面/移动应用框架 | Tauri 用户端 |
| Node.js + ws | WebSocket ↔ TCP 桥接 | 桥接服务器 |

---

## 客户端列表

HyperTicket 提供 5 种客户端接入方式：

### 1. CLI 客户端（C++）

终端交互式命令行工具，最轻量的接入方式。

```bash
# 编译
cmake -S . -B build && cmake --build build -j$(nproc)

# 运行
./bin/client
```

功能：注册、登录、票务浏览、下单、查看订单、取消订单。

### 2. Qt6 QML 管理端（桌面）

基于 Qt 6 + QML 的图形化管理员工具，Material Design 3 风格，直连后端 TCP。

```bash
# 安装 Qt6 依赖
sudo apt install qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window libqt6svg6-dev

# 编译
cd clients/admin-client
cmake -B build && cmake --build build -j$(nproc)

# 运行
./build/bin/HyperTicketAdmin
```

详见 [clients/README.md](clients/README.md)

### 3. Web 前端（React）

React + TypeScript 现代 Web 界面，包含营销落地页、用户端、管理后台。

```bash
# 启动后端 (./bin/ser) + 桥接服务器 + 前端
cd websocket-bridge && npm install && npm start &
cd ../frontend && npm install && npm run dev

# 或一键启动
./scripts/start-frontend.sh
```

页面路由：

| 路径 | 页面 | 说明 |
|------|------|------|
| `/` | 营销落地页 | 品牌展示、功能介绍 |
| `/auth/login` | 登录 | 手机号 + 密码 / token |
| `/auth/register` | 注册 | 新用户注册 |
| `/customer` | 票务浏览 | 搜索、筛选、预订 |
| `/customer/orders` | 我的订单 | 订单列表、取消 |
| `/admin` | 管理仪表盘 | 数据统计 |
| `/admin/tickets` | 票务管理 | 增删改查 |

详见 [frontend/README.md](frontend/README.md) | [DESIGN.md](DESIGN.md)（设计系统）

### 4. Tauri 2 用户端（桌面 + Android）

Rust + React + TypeScript 的桌面和 Android 客户端，Rust 层直连后端 TCP（无需 WebSocket 桥接）。

```bash
cd clients/user-client
npm install
cargo install tauri-cli --version "^2"
rustup target add aarch64-linux-android  # Android 支持

# 开发模式
npm run tauri dev

# 构建 Android APK
npm run tauri android build
```

### 5. WebSocket 桥接（Node.js）

前端 Web 应用无法直连 TCP 端口，通过此桥接服务中转。

```bash
cd websocket-bridge
npm install
npm start     # 监听 ws://localhost:8080
```

环境变量：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `WS_PORT` | 8080 | WebSocket 监听端口 |
| `TCP_HOST` | 127.0.0.1 | 后端 TCP 地址 |
| `TCP_PORT` | 7000 | 后端 TCP 端口 |

详见 [docs/WEBSOCKET_INTEGRATION.md](docs/WEBSOCKET_INTEGRATION.md)

---

## 协议设计

### 通信协议

- **传输层**：TCP，默认 `127.0.0.1:7000`
- **数据格式**：JSON，按 `\n` 换行符分隔
- **序列化库**：jsoncpp

### 操作类型

| type | 名称 | 说明 | 需要 token |
|------|------|------|:----------:|
| 1 | LOGIN | 用户登录，成功后返回 token | |
| 2 | REGISTER | 用户注册 | |
| 3 | EXIT | 退出登录 | |
| 4 | VIEW | 查看在售票务，支持 `keyword`/`city`/`category` 筛选 | |
| 5 | ORDER | 下单预订（`quantity` 1-6 张，创建 PENDING 待支付订单，15 分钟支付窗口） | ✓ |
| 6 | VIEW_MY | 查看本人订单（含 `expire_at` 支付截止、`ticket_price` 单价） | ✓ |
| 7 | CANCEL | 取消预订（PENDING / CONFIRMED 均可取消） | ✓ |
| 8-15 | ADMIN_* | 管理员接口（登录 / 票务与用户管理 / 统计 / 黑名单） | admin_token |
| 16 | DELETE_ORDER | 删除已取消/已过期订单记录 | ✓ |
| 17 | VIEW_SEATS | 查看票务座位图 | |
| 18 | VERIFY_ORDER | 按订单号验票（扫码核销） | |
| 19 | TICKET_DETAIL | 票品详情（简介 / 购票须知 / 艺人 / 城市） | |
| 20 | PAY_ORDER | 发起支付：以 `idempotency_key` 创建支付请求；当前仅支持 `provider=MOCK` | ✓ |
| 21 | FAVORITE | 收藏 / 取消收藏（`action`: add \| remove） | ✓ |
| 22 | VIEW_FAVORITES | 我的收藏（想看）列表 | ✓ |
| 23 | HOT_TICKETS | 热门榜（按有效订单量 TOP N，`limit` 默认 10） | |
| 24 | PAY_QUERY | 查询支付结果（前端发起支付后轮询至终态） | ✓ |

### 请求示例

```json
{"type": 1, "usertel": "13800138000", "passward": "Password123"}
{"type": 4, "keyword": "周杰伦", "city": "北京", "category": "concert"}
{"type": 5, "token": "a1b2c3...", "index": 1, "quantity": 2}
{"type": 20, "token": "a1b2c3...", "index": "81", "provider": "MOCK", "idempotency_key": "checkout-81-attempt-1"}
{"type": 24, "token": "a1b2c3...", "index": "81"}
{"type": 21, "token": "a1b2c3...", "index": "8", "action": "add"}
```

### 待支付订单生命周期（v3 异步支付）

```
ORDER(5) → PENDING（锁库存，expire_at = +15min）
  ├─ PAY_ORDER(20)  → CREATED → PROCESSING → SUCCEEDED/FAILED/CLOSED
  │     └─ 定时结算任务（settle_interval_ms 周期，模拟网关回调）：
  │           ├─ 成功 & 订单仍有效 → payment=SUCCEEDED, 订单=CONFIRMED（出票）
  │           ├─ 成功但订单已失效  → payment=REFUNDED（补偿退款）
  │           └─ 失败              → payment=FAILED, 订单保持 PENDING（可重试）
  ├─ PAY_QUERY(24)  → 前端轮询支付结果 + 订单状态
  ├─ CANCEL(7)      → CANCELLED（立即回补库存；已支付则 REFUNDING → REFUNDED）
  └─ 超时未支付      → 定时任务（30s 周期）标记 EXPIRED + 回补 MySQL 与 Redis 库存
```

> 支付流水的所有状态迁移都是 `WHERE status='PROCESSING'` 的条件 UPDATE，重复结算天然幂等；
> 发起支付/取消/超时回收/结算确认都先锁定 reservation 行，同一订单上的竞争操作被串行化，杜绝超卖与重复扣款。
> 模拟网关参数见 `config.json` 的 `payment` 段（`settle_delay_ms` / `settle_interval_ms` / `success_rate_percent`）。

### 响应格式

成功：`{"status": "OK", ...}`
失败：`{"status": "ERR", "reason": "..."}`

> **安全提示**：ORDER / VIEW_MY / CANCEL 必须携带 `token` 字段，服务端凭 token 识别用户身份，不再信任客户端自报的 `tel`/`usertel`。

---

## 部署指南

### Docker 部署（推荐）

完整部署栈：HyperTicket Server + MySQL 8.0 + Redis 7 + Prometheus + Grafana

```bash
# 基础部署（应用 + 数据库 + Redis）
docker-compose up -d

# 完整部署（含 Prometheus + Grafana 监控）
docker-compose --profile monitoring up -d

# 查看状态
docker-compose ps

# 查看日志
docker-compose logs -f hyperticket

# 健康检查
curl http://localhost:7000/health
```

**服务端口映射**：

| 服务 | 内部端口 | 映射端口 |
|------|----------|----------|
| HyperTicket | 7000 | 7000 |
| MySQL | 3306 | 3306 |
| Redis | 6379 | 6379 |
| Prometheus | 9090 | 9090 |
| Grafana | 3000 | 3000 |

**资源限制**（默认）：
- CPU：2 核上限
- 内存：2G 上限

自定义环境变量：

```bash
DB_PASSWORD=my_secure_pass docker-compose up -d
```

详见各 Docker 配置：

| 组件 | 配置文件 |
|------|----------|
| MySQL | [docker/mysql/my.cnf](docker/mysql/my.cnf) |
| Redis | [docker/redis/redis.conf](docker/redis/redis.conf) |
| Prometheus | [docker/prometheus/prometheus.yml](docker/prometheus/prometheus.yml) |

### 水平扩展

1. 启用 Redis Session：`config.json` 中设置 `redis.enabled: true`
2. 前置 Nginx / HAProxy / Kubernetes Service 负载均衡
3. 所有实例共享同一个 Redis + MySQL

### 安全部署清单

- [ ] 数据库密码使用强密码，不硬编码在配置文件中
- [ ] 生产环境使用独立数据库账号，权限最小化
- [ ] 开启防火墙，仅暴露必要的端口（7000）
- [ ] 启用 Redis 认证和 TLS
- [ ] 管理端 `./bin/admin` 仅供受信运营人员使用
- [ ] `.env` 和 `config.json` 已加入 `.gitignore`，避免密码泄露

---

## 配置文件

配置文件优先级（后覆盖前）：`config.json` → `.env` → 进程环境变量

```bash
# 首次使用
cp config.example.json config.json
# 编辑 config.json 填入实际配置
```

**关键配置项**：

```json
{
  "server": {
    "ip": "0.0.0.0",        "port": 7000,
    "io_threads": 1,         "worker_threads": 8,
    "max_connections": 1000, "max_requests_per_sec": 20
  },
  "db": {
    "host": "127.0.0.1",    "port": 3306,
    "user": "hyperticket",  "password": "your_password",
    "name": "hyperticket",  "pool_size": 20
  },
  "redis": {
    "enabled": false
  },
  "metrics": {
    "enabled": false
  }
}
```

`.env` 覆盖示例（见 `.env.example`）：

```env
DB_HOST=127.0.0.1
DB_PASSWORD=your_password
```

> **已知限制**：`io_threads` 必须设为 `1`。Inet 网络库在多 IO 线程模式下存在连接析构竞态，设为 1 时功能正常（业务仍由多 worker 线程并行处理）。

---

## 数据库

### 初始化

```bash
mysql -u root -p < db/init.sql
```

### 表结构

| 表 | 说明 | 主要字段 |
|----|------|----------|
| `users` | 用户 | tel(唯一), username, password_hash, salt, status, last_login |
| `tickets` | 票务 | title, venue, total_seats, available_seats, event_date, status |
| `reservations` | 订单 | user_id, ticket_id, quantity, status(ENUM), created_at |
| `admins` | 管理员 | username, password_hash, role |
| `reservation_audit` | 审计流水 | reservation_id, action, detail, created_at |

### 索引优化

- 复合索引：用户登录同时检查状态（`tel + status`）
- 覆盖索引：票务列表查询避免回表
- 外键约束 + 行锁（`SELECT ... FOR UPDATE`）防止超卖

---

## 企业级功能

### 1. Redis Session 持久化（可选）

将 Session 持久化到 Redis，支持多实例水平扩展。

```json
{ "redis": { "enabled": true, "host": "127.0.0.1", "port": 6379 } }
```

**实现文件**：
- `backend/Server/include/RedisSessionManager.hpp`
- `backend/Server/src/RedisSessionManager.cpp`
- `backend/Server/src/RedisConnPool.cpp`

**生产要求**：`sudo apt install libhiredis-dev`

详见 [docs/REDIS_SESSION_IMPLEMENTATION.md](docs/REDIS_SESSION_IMPLEMENTATION.md)

### 2. Prometheus Metrics（可选）

暴露 Prometheus 格式监控指标。

```json
{ "metrics": { "enabled": true, "port": 8080 } }
```

**指标列表**：

| 指标 | 类型 | 说明 |
|------|------|------|
| `hyperticket_requests_total` | Counter | 请求总数（按 method + status） |
| `hyperticket_orders_total` | Counter | 订单总数 |
| `hyperticket_sessions_active` | Gauge | 活跃会话数 |
| `hyperticket_db_connections_active` | Gauge | 活跃数据库连接数 |
| `hyperticket_db_connections_idle` | Gauge | 空闲数据库连接数 |
| `hyperticket_errors_total` | Counter | 错误总数（按类型） |

```bash
curl http://localhost:8080/metrics
```

详见 [docs/PROMETHEUS_METRICS_GUIDE.md](docs/PROMETHEUS_METRICS_GUIDE.md)

### 3. 健康检查

**端点**：`GET /health`（端口 7000）

```json
{
  "status": "healthy",
  "checks": {
    "database": {"passed": true, "duration_ms": 2},
    "disk_space": {"passed": true, "usage": "45%"},
    "memory": {"passed": true, "usage": "60%"}
  },
  "uptime_seconds": 120,
  "version": "1.0.0"
}
```

**Kubernetes Probe**：
```yaml
livenessProbe:
  httpGet: { path: /health, port: 7000 }
  initialDelaySeconds: 30
  periodSeconds: 10
```

---

## 测试

零外部依赖的单元测试，通过 CTest 注册：

```bash
cd build && ctest --output-on-failure

# 单独运行
./build/test_buffer
./build/test_session
./build/test_protocol
./build/test_timestamp
./build/test_redis_session
```

| 测试文件 | 覆盖模块 |
|----------|----------|
| `test_buffer.cpp` | Inet Buffer 类 |
| `test_timestamp.cpp` | ChronoLite Timestamp |
| `test_session.cpp` | Server SessionManager |
| `test_protocol.cpp` | JSON 协议解析 |
| `test_redis_session.cpp` | Redis Session 管理 |

### 基准测试

```bash
./scripts/benchmark.sh      # 完整基准测试
./scripts/simple_benchmark.sh # 快速基准测试
```

---

## 项目结构

```
HyperTicket/
├── bin/                       # 编译输出（ser / client / admin）
├── backend/                   # C++ 后端源码
│   ├── Admin/                 # 管理端 CLI 工具
│   ├── ChronoLite/            # 异步日志库
│   ├── Client/                # CLI 客户端
│   ├── Common/                # 统一配置加载
│   ├── Domain/                # 领域层（Service + Repository）
│   ├── FixedThreadPool/       # 固定线程池
│   ├── Inet/                  # epoll Reactor 网络库
│   ├── ScheduledThreadPool/   # 定时任务线程池
│   ├── Server/                # 服务端主程序
│   └── SqlConnPool/           # MySQL 连接池
├── clients/                   # 桌面/移动客户端
│   ├── admin-client/          # Qt6 QML 管理员客户端
│   └── user-client/           # Tauri 2 + React 用户客户端
├── db/                        # 数据库脚本
│   └── init.sql               # 建库建表（单数据源）
├── docker/                    # Docker 配置文件
│   ├── mysql/my.cnf
│   ├── redis/redis.conf
│   ├── prometheus/prometheus.yml
│   └── grafana/
├── docs/                      # 技术文档
├── frontend/                  # React Web 前端
├── reports/                   # AI 日志分析报告输出（gitignored）
├── scripts/                   # 工具脚本
├── tests/                     # 单元测试
├── tools/                     # AI 辅助工具
│   ├── log_ai_detector/       # 日志 AI 检测（Python，监控 ERROR/FATAL → LLM 分析）
│   ├── log-ai-detector-ui/    # 日志检测独立 Web UI（Vite + React）
│   └── doc_qa.py              # 文档问答机器人（分级索引 + 渐进式披露）
├── websocket-bridge/          # WebSocket ↔ TCP 桥接
├── docker-compose.yml         # 完整部署编排
├── Dockerfile                 # 多阶段构建
├── config.example.json        # 配置模板
└── .env.example               # 环境变量模板
```

---

## 文档目录

### 核心文档
| 文档 | 说明 |
|------|------|
| [README.md](README.md) | 项目总览（本文档） |
| [DESIGN.md](DESIGN.md) | 前端设计系统（颜色、字体、布局） |
| [PRODUCT.md](PRODUCT.md) | 产品需求与品牌定位 |
| [CLAUDE.md](CLAUDE.md) | 开发指南（给 AI 使用） |

### 技术文档
| 文档 | 说明 |
|------|------|
| [docs/V2_FEATURES.md](docs/V2_FEATURES.md) | v2 功能升级：详情页 / 搜索筛选 / 待支付 / 收藏 / 热门榜 |
| [docs/WEBSOCKET_INTEGRATION.md](docs/WEBSOCKET_INTEGRATION.md) | WebSocket 桥接集成指南 |
| [docs/PROMETHEUS_METRICS_GUIDE.md](docs/PROMETHEUS_METRICS_GUIDE.md) | Prometheus + Grafana 监控 |
| [docs/REDIS_SESSION_IMPLEMENTATION.md](docs/REDIS_SESSION_IMPLEMENTATION.md) | Redis Session 持久化实现 |
| [docs/REDIS_HIGH_CONCURRENCY.md](docs/REDIS_HIGH_CONCURRENCY.md) | Redis 高并发库存缓存架构 |

### 模块文档
| 文档 | 说明 |
|------|------|
| [backend/Inet/README.md](backend/Inet/README.md) | Inet 网络库 |
| [backend/ChronoLite/README.md](backend/ChronoLite/README.md) | ChronoLite 日志库 |
| [backend/FixedThreadPool/README.md](backend/FixedThreadPool/README.md) | FixedThreadPool |
| [backend/SqlConnPool/README.md](backend/SqlConnPool/README.md) | SqlConnPool 连接池 |
| [backend/Server/README.md](backend/Server/README.md) | 服务端说明 |
| [backend/Client/README.md](backend/Client/README.md) | CLI 客户端说明 |
| [backend/Admin/README.md](backend/Admin/README.md) | 管理端说明 |
| [clients/README.md](clients/README.md) | 桌面客户端说明 |
| [frontend/README.md](frontend/README.md) | Web 前端说明 |
| [tools/log_ai_detector/README.md](tools/log_ai_detector/README.md) | 日志 AI 检测工具 |

---

## AI 辅助工具

### 日志 AI 检测（log_ai_detector）

监控 `logs/*.log` 中的 ERROR/FATAL，自动调用 LLM（默认 DeepSeek `deepseek-v4-flash`）结合代码索引与最近 feat commit diff 定位问题，生成 Markdown 报告并支持 webhook/邮件通知。

```bash
# 构建独立 Web UI（首次）
cd tools/log-ai-detector-ui && npm install && npm run build && cd ../..

# 启动监控 + Web 服务
scripts/log-ai-detector --serve

# 访问 UI（含模型切换、报告查看、单次分析）
# http://localhost:7070/log-ai-detector/
```

配置来自 `.env`（`LOG_AI_*` / `DEEPSEEK_API_KEY`），详见 [tools/log_ai_detector/README.md](tools/log_ai_detector/README.md)。

### 文档问答机器人（doc_qa）

基于分级索引 + 渐进式披露：第一轮 LLM 从文件摘要索引中挑选相关文档，第二轮加载全文回答，文档量增长不影响上下文占用。

```bash
python3 tools/doc_qa.py                             # 交互模式
python3 tools/doc_qa.py -q "Server 的请求处理流程？"  # 单次提问
python3 tools/doc_qa.py --serve                     # Web UI（对话界面 + 文档索引树）
# → http://127.0.0.1:7171/doc-qa/
```

Web UI 首次使用需构建：`cd tools/doc-qa-ui && npm install && npm run build`。
每个回答会标注本轮加载的文档来源。

### codegraph 代码索引（feat commit 触发）

```bash
scripts/update-code-index-on-feat-commit --full          # 全量索引
scripts/update-code-index-on-feat-commit --install-hook  # 安装 post-commit 钩子
```

安装钩子后，每次 `feat:` commit 自动根据 diff 增量更新本地代码索引，供日志分析定位问题使用。

---

## 贡献

1. Fork 本仓库
2. 创建特性分支：`git checkout -b feature/amazing-feature`
3. 提交变更：`git commit -m 'Add amazing feature'`
4. 推送分支：`git push origin feature/amazing-feature`
5. 创建 Pull Request

## 许可证

[MIT](LICENSE) © 2026 shanchuann
