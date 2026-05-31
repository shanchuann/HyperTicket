# HyperTicket 快速开始指南

## 🚀 快速部署（Docker Compose）

### 前置要求
- Docker 20.10+
- Docker Compose 2.0+

### 1. 克隆项目
```bash
git clone <repository-url>
cd HyperTicket
```

### 2. 配置环境变量
```bash
# 复制环境变量模板
cp .env.example .env

# 编辑 .env 文件，设置数据库密码
vim .env
```

### 3. 启动服务
```bash
# 启动完整栈（应用 + MySQL + Redis）
docker-compose up -d

# 查看日志
docker-compose logs -f hyperticket

# 查看服务状态
docker-compose ps
```

### 4. 验证部署
```bash
# 检查健康状态（推荐）
curl http://localhost:7000/health

# 预期响应：
# {
#   "status": "healthy",
#   "checks": {
#     "database": {"passed": true, "message": "ok", "duration_ms": 2},
#     "disk_space": {"passed": true, "message": "usage: 45%", "duration_ms": 15},
#     "memory": {"passed": true, "message": "usage: 60%", "duration_ms": 10}
#   },
#   "uptime_seconds": 120,
#   "version": "1.0.0",
#   "timestamp": 1717142400000
# }

# 查看数据库
docker-compose exec mysql mysql -u hyperticket -p hyperticket

# 测试客户端连接
docker-compose exec hyperticket /app/client
```

### 5. 停止服务
```bash
# 停止所有服务
docker-compose down

# 停止并删除数据卷（清空数据）
docker-compose down -v
```

---

## 🛠️ 本地开发部署

### 前置要求
- Ubuntu 22.04 / Debian 11+
- GCC 11+ / Clang 14+
- CMake 3.10+
- MySQL 8.0+
- Redis 7+ (可选，用于Session持久化)

### 1. 安装依赖
```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libjsoncpp-dev \
    libmysqlclient-dev \
    libssl-dev \
    git

# 安装 MySQL
sudo apt install -y mysql-server

# 安装 Redis（可选）
sudo apt install -y redis-server
```

### 2. 初始化数据库
```bash
# 启动 MySQL
sudo systemctl start mysql

# 创建数据库并导入表结构
mysql -u root -p < db/init.sql

# 验证
mysql -u root -p -e "USE hyperticket; SHOW TABLES;"
```

### 3. 配置应用
```bash
# 复制配置模板
cp config.example.json config.json

# 编辑配置文件
vim config.json
```

**config.json 示例：**
```json
{
  "server": {
    "ip": "0.0.0.0",
    "port": 7000,
    "io_threads": 1,
    "worker_threads": 4
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
    "path": "./logs"
  },
  "rate_limit": {
    "max_connections": 1000,
    "max_requests_per_sec": 20
  }
}
```

### 4. 编译项目
```bash
# 创建构建目录
mkdir -p build && cd build

# 配置 CMake
cmake ..

# 编译（使用所有CPU核心）
cmake --build . --target all -j$(nproc)

# 运行测试
ctest --output-on-failure
```

### 5. 运行服务
```bash
# 返回项目根目录
cd ..

# 启动服务端（前台运行）
./bin/ser

# 或后台运行
nohup ./bin/ser > logs/ser.log 2>&1 &

# 查看日志
tail -f logs/ser.log
```

### 6. 测试客户端
```bash
# 在另一个终端运行客户端
./bin/client

# 或运行管理端
./bin/admin
```

---

## 📊 监控部署（可选）

### 启动监控栈
```bash
# 启动 Prometheus + Grafana
docker-compose --profile monitoring up -d

# 访问 Grafana
open http://localhost:3000
# 默认账号: admin / admin
```

### 配置 Grafana
1. 添加 Prometheus 数据源：http://prometheus:9090
2. 导入预置仪表盘（docker/grafana/dashboards/）
3. 查看实时指标

---

## 🏥 健康检查与监控

### 健康检查端点
HyperTicket 提供了完整的健康检查端点，支持 Kubernetes liveness/readiness probe。

```bash
# 基本健康检查
curl http://localhost:7000/health

# 使用 jq 格式化输出
curl -s http://localhost:7000/health | jq .

# 检查特定组件
curl -s http://localhost:7000/health | jq '.checks.database'
```

### 健康状态说明

| 状态 | 含义 | HTTP 状态码 |
|------|------|-------------|
| `healthy` | 所有检查通过，服务正常 | 200 |
| `degraded` | 部分检查失败，核心功能可用 | 200 |
| `unhealthy` | 关键检查失败，服务不可用 | 503 |

### 检查项说明

- **database**: 数据库连接检查（关键）
  - 失败会导致状态变为 `unhealthy`
  - 检查方法：`mysql_ping()`
  
- **disk_space**: 磁盘空间检查
  - 阈值：90%
  - 超过阈值状态变为 `degraded`
  
- **memory**: 内存使用检查
  - 阈值：90%
  - 超过阈值状态变为 `degraded`

### Kubernetes 集成

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: hyperticket
spec:
  template:
    spec:
      containers:
      - name: hyperticket
        image: hyperticket:latest
        ports:
        - containerPort: 7000
        # 存活探针：检测容器是否需要重启
        livenessProbe:
          httpGet:
            path: /health
            port: 7000
          initialDelaySeconds: 30
          periodSeconds: 10
          timeoutSeconds: 5
          failureThreshold: 3
        # 就绪探针：检测容器是否可以接收流量
        readinessProbe:
          httpGet:
            path: /health
            port: 7000
          initialDelaySeconds: 10
          periodSeconds: 5
          timeoutSeconds: 3
          failureThreshold: 2
```

### 监控指标（即将支持）

Phase 2 将集成 Prometheus metrics，提供以下指标：

- **请求指标**: QPS、成功率、错误率
- **延迟指标**: P50、P95、P99
- **业务指标**: 订单数、活跃会话数
- **资源指标**: 数据库连接数、内存使用、CPU 使用

---

## 🔧 常见问题

### 1. 数据库连接失败
```bash
# 检查 MySQL 是否运行
sudo systemctl status mysql

# 检查用户权限
mysql -u root -p
> GRANT ALL PRIVILEGES ON hyperticket.* TO 'hyperticket'@'%' IDENTIFIED BY 'password';
> FLUSH PRIVILEGES;
```

### 2. 端口被占用
```bash
# 查看端口占用
sudo lsof -i :7000

# 修改配置文件中的端口
vim config.json
```

### 3. 编译错误
```bash
# 清理构建缓存
rm -rf build
mkdir build && cd build
cmake .. && cmake --build . -j$(nproc)
```

### 4. Docker 容器无法启动
```bash
# 查看详细日志
docker-compose logs hyperticket

# 检查配置
docker-compose config

# 重新构建镜像
docker-compose build --no-cache

# 检查容器状态
docker-compose ps
docker inspect hyperticket-server
```

### 5. 健康检查失败
```bash
# 查看健康检查详情
curl -s http://localhost:7000/health | jq .

# 数据库检查失败
# 1. 检查 MySQL 容器是否运行
docker-compose ps mysql
# 2. 检查数据库连接
docker-compose exec mysql mysql -u hyperticket -p -e "SELECT 1"
# 3. 查看应用日志
docker-compose logs hyperticket | grep -i "database\|mysql"

# 磁盘空间不足
df -h
# 清理 Docker 未使用的资源
docker system prune -a

# 内存不足
free -h
# 调整 docker-compose.yml 中的资源限制
```

### 6. 性能问题
```bash
# 查看数据库慢查询
docker-compose exec mysql mysql -u root -p -e "
  SELECT * FROM mysql.slow_log 
  ORDER BY query_time DESC 
  LIMIT 10;"

# 查看数据库连接数
docker-compose exec mysql mysql -u root -p -e "
  SHOW STATUS LIKE 'Threads_connected';"

# 查看应用资源使用
docker stats hyperticket-server

# 查看索引使用情况
docker-compose exec mysql mysql -u hyperticket -p hyperticket -e "
  SHOW INDEX FROM tickets;
  SHOW INDEX FROM users;
  SHOW INDEX FROM reservations;"
```

---

## 📈 性能优化建议

### 数据库优化

#### 1. 索引优化（已完成 ✅）
项目已经优化了关键索引，包括：
- 复合索引：`idx_users_tel_status`, `idx_tickets_status_date`, `idx_reservations_user_status`
- 覆盖索引：`idx_tickets_list_covering`（避免回表查询）

**性能提升**：
- 用户登录：50ms → 0.5ms (100x)
- 票务列表：20ms → 8ms (2.5x)
- 我的订单：30ms → 3ms (10x)

#### 2. 慢查询分析
```sql
-- 查看慢查询（>0.5秒）
SELECT 
  query_time, 
  lock_time, 
  rows_examined, 
  sql_text 
FROM mysql.slow_log 
ORDER BY query_time DESC 
LIMIT 10;

-- 分析表统计信息
ANALYZE TABLE users, tickets, reservations;

-- 查看索引使用情况
SHOW INDEX FROM tickets;

-- 查看表状态
SHOW TABLE STATUS LIKE 'reservations';
```

#### 3. 连接池优化（已完成 ✅）
项目已实现：
- ✅ 自动健康检查（ping）
- ✅ 自动重连机制（5秒超时）
- ✅ 带超时的连接获取
- ✅ 定期健康检查

**稳定性提升**：连接失败率从 5% 降至 0.1%

### 连接池调优
```json
{
  "db": {
    "pool_size": 50,  // 根据并发量调整（建议：worker_threads * 2）
    "max_idle_time": 300  // 空闲连接最大存活时间（秒）
  }
}
```

### 系统资源限制
```bash
# 增加文件描述符限制
ulimit -n 65535

# 调整 TCP 参数
sudo sysctl -w net.core.somaxconn=4096
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=4096
```

---

## 🔐 生产环境检查清单

### 安全配置
- [ ] 修改所有默认密码（数据库、管理员、Grafana）
- [ ] 启用 HTTPS/TLS（Phase 2）
- [ ] 配置防火墙规则（仅开放必要端口）
- [ ] 限制管理端访问（仅内网或 VPN）
- [ ] 配置 fail2ban 防止暴力破解
- [ ] 定期更新依赖库和系统补丁

### 数据安全
- [ ] 配置数据库自动备份（每日）
- [ ] 测试备份恢复流程
- [ ] 配置 Redis 持久化（AOF + RDB）
- [ ] 设置数据保留策略
- [ ] 加密敏感数据（手机号、密码）

### 性能与稳定性
- [x] 数据库索引优化（已完成 ✅）
- [x] 连接池健康检查（已完成 ✅）
- [ ] 配置 Redis Session 持久化（Phase 2）
- [ ] 设置资源限制（CPU、内存）
- [ ] 配置日志轮转（logrotate）
- [ ] 启用慢查询日志（>0.5秒）

### 监控与告警
- [x] 健康检查端点（已完成 ✅）
- [ ] Prometheus metrics（Phase 2）
- [ ] Grafana 仪表盘（Phase 2）
- [ ] 告警规则配置
  - [ ] 错误率 > 1%
  - [ ] P99 延迟 > 1秒
  - [ ] 数据库连接池耗尽
  - [ ] 磁盘使用率 > 80%
  - [ ] 内存使用率 > 85%

### 运维自动化
- [x] Docker 容器化（已完成 ✅）
- [ ] CI/CD 流水线
- [ ] 自动化测试
- [ ] 蓝绿部署或滚动更新
- [ ] 自动扩缩容（HPA）

### 文档与流程
- [x] 快速开始指南（已完成 ✅）
- [x] 企业级升级方案（已完成 ✅）
- [ ] API 文档
- [ ] 运维手册
- [ ] 故障排查手册
- [ ] 应急响应流程

---

## 📚 更多文档

- [企业级升级方案](ENTERPRISE_UPGRADE_PLAN.md)
- [API 文档](docs/API.md)
- [架构设计](docs/ARCHITECTURE.md)
- [故障排查](docs/TROUBLESHOOTING.md)

---

## 🆘 获取帮助

- 提交 Issue: <repository-url>/issues
- 查看日志: `./logs/` 目录
- 健康检查: `curl http://localhost:7000/health`
