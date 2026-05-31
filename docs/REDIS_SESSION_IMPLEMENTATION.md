# Redis Session 持久化实现指南

## 概述

HyperTicket 现已支持 Redis Session 持久化，可实现多实例部署的会话共享。当前实现包含两种模式：

1. **占位模式**（默认）：使用文件模拟 Redis，用于开发和测试
2. **生产模式**：使用真实的 Redis，需要安装 hiredis 库

## 架构设计

### 核心组件

- **RedisConnPool**：Redis 连接池，管理连接的获取、释放和健康检查
- **RedisSessionManager**：Session 管理器，实现 ISessionManager 接口
- **RedisConnGuard**：RAII 包装器，自动释放连接

### 数据格式

- **Key 格式**：`session:{token}`
- **Value 格式**：JSON `{tel, userId, expireMs}`
- **TTL**：30 分钟自动过期（可配置）

### 特性

- 连接池复用，减少连接开销
- 自动健康检查和重连
- 滑动续期机制
- 线程安全
- 支持并发访问

## 安装 hiredis（生产环境）

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install libhiredis-dev
```

### CentOS/RHEL

```bash
sudo yum install hiredis-devel
```

### 从源码编译

```bash
git clone https://github.com/redis/hiredis.git
cd hiredis
make
sudo make install
sudo ldconfig
```

## 安装 Redis Server

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install redis-server
sudo systemctl start redis-server
sudo systemctl enable redis-server
```

### 验证 Redis 运行

```bash
redis-cli ping
# 应该返回: PONG
```

## 编译项目

### 重新配置和编译

```bash
cd /path/to/HyperTicket
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### 检查 hiredis 是否被检测到

编译时应该看到：

```
-- Found hiredis: /usr/lib/x86_64-linux-gnu/libhiredis.so
```

而不是：

```
CMake Warning: hiredis not found, Redis Session will use placeholder implementation
```

## 配置

### config.json

```json
{
  "redis": {
    "enabled": true,
    "host": "127.0.0.1",
    "port": 6379,
    "pool_size": 4,
    "session_ttl_minutes": 30
  }
}
```

### 环境变量（可选）

```bash
export REDIS_HOST=127.0.0.1
export REDIS_PORT=6379
```

## 运行测试

### 单元测试

```bash
cd build
./bin/test_redis_session
```

### 集成测试

确保 Redis 服务器正在运行：

```bash
# 启动 Redis
sudo systemctl start redis-server

# 运行测试
./bin/test_redis_session

# 查看 Redis 中的 session
redis-cli KEYS "session:*"
redis-cli GET "session:xxxxx"
```

## 使用示例

### 在代码中使用

```cpp
#include "RedisSessionManager.hpp"

// 创建 Redis Session Manager
auto sessionMgr = std::make_unique<hyperticket::RedisSessionManager>(
    "127.0.0.1",  // Redis host
    6379,         // Redis port
    30 * 60 * 1000, // TTL: 30 minutes
    4             // 连接池大小
);

// 创建 session
std::string token = sessionMgr->create("13800138000", 1001, nowMs());

// 解析 session
std::string tel;
int64_t userId;
bool ok = sessionMgr->resolve(token, nowMs(), tel, userId);

// 删除 session
sessionMgr->remove(token);

// 健康检查
bool healthy = sessionMgr->ping();
```

## 性能优化

### 连接池大小

根据并发请求数调整连接池大小：

- 低并发（< 100 QPS）：2-4 个连接
- 中并发（100-1000 QPS）：4-8 个连接
- 高并发（> 1000 QPS）：8-16 个连接

### Redis 配置优化

编辑 `/etc/redis/redis.conf`：

```conf
# 最大连接数
maxclients 10000

# 内存限制
maxmemory 256mb
maxmemory-policy allkeys-lru

# 持久化（可选，session 数据可以不持久化）
save ""
appendonly no
```

## 多实例部署

### 架构

```
                    Nginx/HAProxy
                          |
        +-----------------+-----------------+
        |                 |                 |
   Instance 1        Instance 2        Instance 3
        |                 |                 |
        +-----------------+-----------------+
                          |
                    Redis Server
                  (共享 Session)
```

### 部署步骤

1. 部署 Redis 服务器（可以是 Redis Cluster 或 Redis Sentinel）
2. 配置所有 HyperTicket 实例连接到同一个 Redis
3. 配置负载均衡器（Nginx/HAProxy）
4. 启动所有实例

### Nginx 配置示例

```nginx
upstream hyperticket {
    least_conn;
    server 192.168.1.10:7000;
    server 192.168.1.11:7000;
    server 192.168.1.12:7000;
}

server {
    listen 80;
    server_name api.hyperticket.com;

    location / {
        proxy_pass http://hyperticket;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

## 监控

### Redis 监控指标

```bash
# 连接数
redis-cli INFO clients | grep connected_clients

# 内存使用
redis-cli INFO memory | grep used_memory_human

# Session 数量
redis-cli DBSIZE

# 命令统计
redis-cli INFO stats | grep instantaneous_ops_per_sec
```

### 应用监控

在 Prometheus Metrics 中添加：

- `redis_session_create_total`：创建 session 总数
- `redis_session_resolve_total`：解析 session 总数
- `redis_session_resolve_failed_total`：解析失败总数
- `redis_connection_pool_size`：连接池大小
- `redis_connection_pool_active`：活跃连接数

## 故障排查

### 问题：连接 Redis 失败

```bash
# 检查 Redis 是否运行
sudo systemctl status redis-server

# 检查端口是否监听
sudo netstat -tlnp | grep 6379

# 测试连接
redis-cli -h 127.0.0.1 -p 6379 ping
```

### 问题：Session 丢失

- 检查 Redis 内存是否充足
- 检查 TTL 配置是否正确
- 检查 Redis 是否被重启

### 问题：性能下降

- 增加连接池大小
- 检查 Redis 慢查询日志：`redis-cli SLOWLOG GET 10`
- 考虑使用 Redis Cluster 分片

## 从内存 Session 迁移

### 平滑迁移步骤

1. 部署 Redis 服务器
2. 配置 `redis.enabled: true`
3. 重启服务（会话会丢失，用户需要重新登录）
4. 验证 Redis 中有 session 数据

### 零停机迁移（高级）

1. 实现双写：同时写入内存和 Redis
2. 读取时优先从 Redis 读，失败则从内存读
3. 观察一段时间后，移除内存 Session
4. 切换到纯 Redis 模式

## 参考资料

- [hiredis GitHub](https://github.com/redis/hiredis)
- [Redis 官方文档](https://redis.io/documentation)
- [Redis 最佳实践](https://redis.io/topics/best-practices)
