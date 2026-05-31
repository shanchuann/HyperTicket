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
# 检查服务是否运行
curl http://localhost:7000/health

# 查看数据库
docker-compose exec mysql mysql -u hyperticket -p hyperticket
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
```

---

## 📈 性能优化建议

### 数据库优化
```sql
-- 查看慢查询
SELECT * FROM mysql.slow_log ORDER BY query_time DESC LIMIT 10;

-- 分析表
ANALYZE TABLE users, tickets, reservations;

-- 查看索引使用情况
SHOW INDEX FROM tickets;
```

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

- [ ] 修改所有默认密码（数据库、管理员、Grafana）
- [ ] 启用 HTTPS/TLS
- [ ] 配置防火墙规则
- [ ] 设置日志轮转
- [ ] 配置自动备份（数据库）
- [ ] 设置监控告警
- [ ] 限制管理端访问（仅内网）
- [ ] 定期更新依赖库
- [ ] 配置 Redis 持久化
- [ ] 设置资源限制（CPU、内存）

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
