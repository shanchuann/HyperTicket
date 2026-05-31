# Redis Session 持久化集成指南

## 概述

将内存 SessionManager 升级为 Redis 持久化，支持多实例水平扩展。

## 为什么需要 Redis Session

### 当前问题（内存 Session）
- ❌ Session 存储在单个进程内存中
- ❌ 无法支持多实例部署（负载均衡）
- ❌ 进程重启后所有用户需要重新登录
- ❌ 无法实现跨服务器的 Session 共享

### Redis Session 的优势
- ✅ 支持多实例部署（水平扩展）
- ✅ Session 持久化，进程重启不丢失
- ✅ 自动过期（TTL），无需手动清理
- ✅ 高性能（内存数据库）
- ✅ 支持主从复制和集群模式

## 实施方案

### 1. 选择 Redis 客户端库

#### 方案 A：hiredis（推荐）
**优点：**
- 官方 C 客户端，稳定可靠
- 轻量级，依赖少
- 性能优秀

**缺点：**
- API 较底层，需要手动管理内存
- 不支持连接池（需自己实现）

**安装：**
```bash
sudo apt install libhiredis-dev
```

**CMakeLists.txt：**
```cmake
find_library(HIREDIS_LIB hiredis)
target_link_libraries(ser PRIVATE ${HIREDIS_LIB})
```

#### 方案 B：redis-plus-plus（推荐）
**优点：**
- C++ 封装，API 友好
- 内置连接池
- 支持 Pipeline、Transaction、Pub/Sub
- 异常安全

**缺点：**
- 需要额外安装
- 依赖 hiredis

**安装：**
```bash
# 安装 hiredis
sudo apt install libhiredis-dev

# 编译安装 redis-plus-plus
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build && cd build
cmake ..
make
sudo make install
```

**CMakeLists.txt：**
```cmake
find_package(redis++ REQUIRED)
target_link_libraries(ser PRIVATE redis++::redis++_static)
```

### 2. 实现 RedisSessionManager

#### 核心接口

```cpp
class RedisSessionManager : public ISessionManager
{
public:
    RedisSessionManager(const std::string &host, int port, int64_t ttlMs);
    
    // 创建 session：生成 token，存储到 Redis
    std::string create(const std::string &tel, int64_t userId, int64_t nowMs) override;
    
    // 解析 session：从 Redis 读取，验证并续期
    bool resolve(const std::string &token, int64_t nowMs,
                 std::string &telOut, int64_t &userIdOut) override;
    
    // 清理过期 session：Redis 自动过期，无需实现
    void purgeExpired(int64_t nowMs) override {}
    
    // 删除 session：登出时调用
    void remove(const std::string &token);
};
```

#### Redis 数据结构

**Key 格式：**
```
session:{token}
```

**Value 格式（JSON）：**
```json
{
  "tel": "13812345678",
  "userId": 123,
  "expireMs": 1717142400000
}
```

**TTL：**
- 30 分钟（1800 秒）
- 每次 resolve 时自动续期

#### 示例实现（使用 redis-plus-plus）

```cpp
#include <sw/redis++/redis++.h>
#include <jsoncpp/json/json.h>

using namespace sw::redis;

class RedisSessionManager : public ISessionManager
{
public:
    RedisSessionManager(const std::string &host, int port, int64_t ttlMs)
        : ttlMs_(ttlMs)
    {
        ConnectionOptions opts;
        opts.host = host;
        opts.port = port;
        opts.socket_timeout = std::chrono::milliseconds(100);
        
        ConnectionPoolOptions poolOpts;
        poolOpts.size = 10;  // 连接池大小
        
        redis_ = std::make_unique<Redis>(opts, poolOpts);
        LOG_INFO << "Redis connected: " << host << ":" << port;
    }
    
    std::string create(const std::string &tel, int64_t userId, int64_t nowMs) override
    {
        std::string token = generateToken();
        std::string key = "session:" + token;
        
        // 构造 JSON
        Json::Value value;
        value["tel"] = tel;
        value["userId"] = static_cast<Json::Int64>(userId);
        value["expireMs"] = static_cast<Json::Int64>(nowMs + ttlMs_);
        
        Json::StreamWriterBuilder builder;
        std::string jsonStr = Json::writeString(builder, value);
        
        // SETEX：设置值并指定过期时间
        auto ttlSeconds = std::chrono::seconds(ttlMs_ / 1000);
        redis_->setex(key, ttlSeconds, jsonStr);
        
        return token;
    }
    
    bool resolve(const std::string &token, int64_t nowMs,
                 std::string &telOut, int64_t &userIdOut) override
    {
        std::string key = "session:" + token;
        
        // GET：获取值
        auto val = redis_->get(key);
        if (!val) {
            return false;  // key 不存在或已过期
        }
        
        // 解析 JSON
        Json::CharReaderBuilder builder;
        Json::Value value;
        std::istringstream iss(*val);
        std::string errs;
        if (!Json::parseFromStream(builder, iss, &value, &errs)) {
            LOG_ERROR << "JSON parse failed: " << errs;
            return false;
        }
        
        // 检查过期（双重保险）
        int64_t expireMs = value["expireMs"].asInt64();
        if (expireMs <= nowMs) {
            redis_->del(key);
            return false;
        }
        
        telOut = value["tel"].asString();
        userIdOut = value["userId"].asInt64();
        
        // 续期
        auto ttlSeconds = std::chrono::seconds(ttlMs_ / 1000);
        redis_->expire(key, ttlSeconds);
        
        return true;
    }
    
    void purgeExpired(int64_t nowMs) override
    {
        // Redis 自动过期，无需手动清理
    }
    
    void remove(const std::string &token)
    {
        std::string key = "session:" + token;
        redis_->del(key);
    }

private:
    std::unique_ptr<Redis> redis_;
    int64_t ttlMs_;
};
```

### 3. 配置更新

**config.json：**
```json
{
  "server": {
    "ip": "0.0.0.0",
    "port": 7000
  },
  "db": {
    "host": "127.0.0.1",
    "port": 3306,
    "user": "hyperticket",
    "password": "password",
    "name": "hyperticket",
    "pool_size": 20
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "pool_size": 10,
    "session_ttl_minutes": 30
  }
}
```

### 4. 服务端集成

**Server/src/ser.cpp：**
```cpp
#include "../include/RedisSessionManager.hpp"

int main()
{
    // 加载配置
    hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json");
    
    // 创建 Redis Session Manager
    hyperticket::RedisSessionManager sessionMgr(
        cfg.redis.host,
        cfg.redis.port,
        cfg.redis.sessionTtlMinutes * 60 * 1000
    );
    
    // 创建 TicketService
    hyperticket::TicketService ticketSvc(&connPool, &sessionMgr);
    
    // ... 其他初始化
}
```

## 性能优化

### 1. 连接池
- 使用连接池复用 Redis 连接
- 池大小：10-20 个连接

### 2. Pipeline
- 批量操作使用 Pipeline 减少网络往返

### 3. 序列化优化
- 考虑使用 MessagePack 替代 JSON（更小更快）

### 4. 缓存策略
- 本地缓存热点 session（LRU）
- 减少 Redis 访问频率

## 监控指标

### Redis 指标
- 连接数
- 命令执行时间
- 内存使用
- Key 数量
- 过期 Key 数量

### Session 指标
- 创建速率
- 验证速率
- 命中率
- 平均 TTL

## 故障处理

### Redis 不可用
- 降级到内存 SessionManager
- 返回错误，提示用户重新登录

### Redis 主从切换
- 使用 Redis Sentinel 自动故障转移
- 客户端自动重连

## 测试验证

### 单元测试
```cpp
TEST(RedisSessionManager, CreateAndResolve)
{
    RedisSessionManager mgr("127.0.0.1", 6379, 30 * 60 * 1000);
    
    // 创建 session
    std::string token = mgr.create("13812345678", 123, nowMs());
    ASSERT_FALSE(token.empty());
    
    // 解析 session
    std::string tel;
    int64_t userId;
    ASSERT_TRUE(mgr.resolve(token, nowMs(), tel, userId));
    ASSERT_EQ(tel, "13812345678");
    ASSERT_EQ(userId, 123);
    
    // 删除 session
    mgr.remove(token);
    ASSERT_FALSE(mgr.resolve(token, nowMs(), tel, userId));
}
```

### 集成测试
- 多实例部署测试
- 负载均衡测试
- 故障转移测试

## 部署建议

### 开发环境
- 单机 Redis
- 无持久化

### 生产环境
- Redis 主从复制
- Redis Sentinel 高可用
- AOF + RDB 持久化
- 定期备份

## 预期效果

- ✅ 支持多实例水平扩展
- ✅ Session 持久化，进程重启不丢失
- ✅ 性能：< 1ms 延迟
- ✅ 可用性：99.9%+

## 参考资料

- [hiredis GitHub](https://github.com/redis/hiredis)
- [redis-plus-plus GitHub](https://github.com/sewenew/redis-plus-plus)
- [Redis 官方文档](https://redis.io/documentation)
