# Prometheus Metrics 监控集成指南

## 概述

为 HyperTicket 集成 Prometheus metrics，提供完整的业务和系统监控能力。

## 为什么需要 Metrics 监控

### 当前问题
- ❌ 无法实时了解系统运行状态
- ❌ 性能问题难以定位
- ❌ 缺少告警机制
- ❌ 容量规划缺少数据支持

### Metrics 的价值
- ✅ 实时监控系统健康状态
- ✅ 快速定位性能瓶颈
- ✅ 主动告警，及时响应
- ✅ 数据驱动的容量规划
- ✅ 可视化仪表盘

## 指标设计

### 1. 请求指标（Request Metrics）

#### 请求总数
```cpp
// Counter: 单调递增
hyperticket_requests_total{method="login", status="success"} 1234
hyperticket_requests_total{method="login", status="failed"} 56
hyperticket_requests_total{method="order", status="success"} 890
```

#### 请求延迟
```cpp
// Histogram: 分布统计
hyperticket_request_duration_seconds{method="login", le="0.001"} 100
hyperticket_request_duration_seconds{method="login", le="0.01"} 500
hyperticket_request_duration_seconds{method="login", le="0.1"} 980
hyperticket_request_duration_seconds{method="login", le="+Inf"} 1000
```

### 2. 业务指标（Business Metrics）

#### 订单指标
```cpp
// Counter: 订单总数
hyperticket_orders_total{status="success"} 5678
hyperticket_orders_total{status="failed"} 123

// Gauge: 当前活跃订单
hyperticket_orders_active 45
```

#### 会话指标
```cpp
// Gauge: 活跃会话数
hyperticket_sessions_active 234

// Counter: 会话创建总数
hyperticket_sessions_created_total 12345
```

### 3. 资源指标（Resource Metrics）

#### 数据库连接池
```cpp
// Gauge: 活跃连接数
hyperticket_db_connections_active 15

// Gauge: 空闲连接数
hyperticket_db_connections_idle 5

// Counter: 连接获取失败次数
hyperticket_db_connection_errors_total 3
```

#### 系统资源
```cpp
// Gauge: 内存使用（字节）
hyperticket_memory_usage_bytes 524288000

// Gauge: CPU 使用率（0-1）
hyperticket_cpu_usage_ratio 0.65
```

### 4. 错误指标（Error Metrics）

```cpp
// Counter: 错误总数（按类型分类）
hyperticket_errors_total{type="db_unavailable"} 5
hyperticket_errors_total{type="invalid_credentials"} 23
hyperticket_errors_total{type="rate_limited"} 12
```

## 实施方案

### 方案选择：prometheus-cpp

**优点：**
- 官方推荐的 C++ 客户端
- 支持 Counter、Gauge、Histogram、Summary
- 线程安全
- 性能优秀

**安装：**
```bash
# Ubuntu
sudo apt install libprometheus-cpp-dev

# 或从源码编译
git clone https://github.com/jupp0r/prometheus-cpp.git
cd prometheus-cpp
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON
make -j$(nproc)
sudo make install
```

**CMakeLists.txt：**
```cmake
find_package(prometheus-cpp CONFIG REQUIRED)
target_link_libraries(ser PRIVATE 
    prometheus-cpp::core
    prometheus-cpp::pull
)
```

## 代码实现

### 1. Metrics 管理器

```cpp
// Server/include/MetricsManager.hpp
#ifndef HYPERTICKET_METRICS_MANAGER_HPP
#define HYPERTICKET_METRICS_MANAGER_HPP

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <memory>
#include <string>

namespace hyperticket
{
    class MetricsManager
    {
    public:
        explicit MetricsManager(int port = 8080);
        
        // 请求指标
        void recordRequest(const std::string &method, const std::string &status);
        void recordRequestDuration(const std::string &method, double seconds);
        
        // 业务指标
        void recordOrder(const std::string &status);
        void setActiveOrders(int count);
        void setActiveSessions(int count);
        
        // 资源指标
        void setDbConnectionsActive(int count);
        void setDbConnectionsIdle(int count);
        void recordDbConnectionError();
        
        // 错误指标
        void recordError(const std::string &type);
        
        // 获取 Registry（用于 Exposer）
        std::shared_ptr<prometheus::Registry> getRegistry() { return registry_; }

    private:
        std::shared_ptr<prometheus::Registry> registry_;
        std::unique_ptr<prometheus::Exposer> exposer_;
        
        // 请求指标
        prometheus::Family<prometheus::Counter> &requestsTotal_;
        prometheus::Family<prometheus::Histogram> &requestDuration_;
        
        // 业务指标
        prometheus::Family<prometheus::Counter> &ordersTotal_;
        prometheus::Family<prometheus::Gauge> &ordersActive_;
        prometheus::Family<prometheus::Gauge> &sessionsActive_;
        
        // 资源指标
        prometheus::Family<prometheus::Gauge> &dbConnectionsActive_;
        prometheus::Family<prometheus::Gauge> &dbConnectionsIdle_;
        prometheus::Family<prometheus::Counter> &dbConnectionErrors_;
        
        // 错误指标
        prometheus::Family<prometheus::Counter> &errorsTotal_;
    };
}

#endif
```

### 2. 实现示例

```cpp
// Server/src/MetricsManager.cpp
#include "../include/MetricsManager.hpp"

namespace hyperticket
{
    MetricsManager::MetricsManager(int port)
        : registry_(std::make_shared<prometheus::Registry>()),
          exposer_(std::make_unique<prometheus::Exposer>("0.0.0.0:" + std::to_string(port))),
          requestsTotal_(prometheus::BuildCounter()
                            .Name("hyperticket_requests_total")
                            .Help("Total number of requests")
                            .Register(*registry_)),
          requestDuration_(prometheus::BuildHistogram()
                              .Name("hyperticket_request_duration_seconds")
                              .Help("Request duration in seconds")
                              .Register(*registry_)),
          ordersTotal_(prometheus::BuildCounter()
                          .Name("hyperticket_orders_total")
                          .Help("Total number of orders")
                          .Register(*registry_)),
          ordersActive_(prometheus::BuildGauge()
                           .Name("hyperticket_orders_active")
                           .Help("Number of active orders")
                           .Register(*registry_)),
          sessionsActive_(prometheus::BuildGauge()
                             .Name("hyperticket_sessions_active")
                             .Help("Number of active sessions")
                             .Register(*registry_)),
          dbConnectionsActive_(prometheus::BuildGauge()
                                  .Name("hyperticket_db_connections_active")
                                  .Help("Number of active database connections")
                                  .Register(*registry_)),
          dbConnectionsIdle_(prometheus::BuildGauge()
                                .Name("hyperticket_db_connections_idle")
                                .Help("Number of idle database connections")
                                .Register(*registry_)),
          dbConnectionErrors_(prometheus::BuildCounter()
                                 .Name("hyperticket_db_connection_errors_total")
                                 .Help("Total number of database connection errors")
                                 .Register(*registry_)),
          errorsTotal_(prometheus::BuildCounter()
                          .Name("hyperticket_errors_total")
                          .Help("Total number of errors by type")
                          .Register(*registry_))
    {
        // 注册 registry 到 exposer
        exposer_->RegisterCollectable(registry_);
        
        LOG_INFO << "Metrics exposed on port " << port << " at /metrics";
    }
    
    void MetricsManager::recordRequest(const std::string &method, const std::string &status)
    {
        requestsTotal_.Add({{"method", method}, {"status", status}}).Increment();
    }
    
    void MetricsManager::recordRequestDuration(const std::string &method, double seconds)
    {
        // Histogram buckets: 1ms, 10ms, 100ms, 1s, 10s
        static const std::vector<double> buckets = {0.001, 0.01, 0.1, 1.0, 10.0};
        requestDuration_.Add({{"method", method}}, buckets).Observe(seconds);
    }
    
    void MetricsManager::recordOrder(const std::string &status)
    {
        ordersTotal_.Add({{"status", status}}).Increment();
    }
    
    void MetricsManager::setActiveOrders(int count)
    {
        ordersActive_.Add({}).Set(count);
    }
    
    void MetricsManager::setActiveSessions(int count)
    {
        sessionsActive_.Add({}).Set(count);
    }
    
    void MetricsManager::setDbConnectionsActive(int count)
    {
        dbConnectionsActive_.Add({}).Set(count);
    }
    
    void MetricsManager::setDbConnectionsIdle(int count)
    {
        dbConnectionsIdle_.Add({}).Set(count);
    }
    
    void MetricsManager::recordDbConnectionError()
    {
        dbConnectionErrors_.Add({}).Increment();
    }
    
    void MetricsManager::recordError(const std::string &type)
    {
        errorsTotal_.Add({{"type", type}}).Increment();
    }
}
```

### 3. 集成到服务端

```cpp
// Server/src/ser.cpp
#include "../include/MetricsManager.hpp"

int main()
{
    // ... 其他初始化
    
    // 创建 Metrics Manager（暴露在 8080 端口）
    hyperticket::MetricsManager metrics(8080);
    
    // 在请求处理中记录指标
    auto handleRequest = [&](const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        auto start = std::chrono::steady_clock::now();
        
        // 解析请求
        Json::Value req = parseRequest(buf);
        std::string method = getMethodName(req["type"].asInt());
        
        // 处理请求
        Json::Value resp = ticketSvc.handle(req);
        
        // 记录指标
        auto end = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        
        std::string status = resp["code"].asInt() == 0 ? "success" : "failed";
        metrics.recordRequest(method, status);
        metrics.recordRequestDuration(method, duration);
        
        if (status == "failed") {
            metrics.recordError(resp["msg"].asString());
        }
        
        // 发送响应
        sendJson(conn, resp);
    };
    
    // 定时更新资源指标
    scheduler.addRunEvery(5000, [&]() {
        metrics.setDbConnectionsActive(connPool.GetActiveConn());
        metrics.setDbConnectionsIdle(connPool.GetFreeConn());
        metrics.setActiveSessions(sessionMgr.size());
    });
    
    // ... 启动服务
}
```

## Prometheus 配置

### prometheus.yml
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'hyperticket'
    static_configs:
      - targets: ['hyperticket:8080']
    metrics_path: '/metrics'
    scrape_interval: 10s
```

## Grafana 仪表盘

### 关键面板

#### 1. QPS 面板
```promql
# 每秒请求数
rate(hyperticket_requests_total[1m])

# 按方法分组
sum(rate(hyperticket_requests_total[1m])) by (method)
```

#### 2. 延迟面板
```promql
# P50 延迟
histogram_quantile(0.50, rate(hyperticket_request_duration_seconds_bucket[1m]))

# P95 延迟
histogram_quantile(0.95, rate(hyperticket_request_duration_seconds_bucket[1m]))

# P99 延迟
histogram_quantile(0.99, rate(hyperticket_request_duration_seconds_bucket[1m]))
```

#### 3. 错误率面板
```promql
# 错误率
sum(rate(hyperticket_requests_total{status="failed"}[1m])) 
/ 
sum(rate(hyperticket_requests_total[1m]))
```

#### 4. 数据库连接池面板
```promql
# 活跃连接
hyperticket_db_connections_active

# 空闲连接
hyperticket_db_connections_idle

# 连接错误率
rate(hyperticket_db_connection_errors_total[1m])
```

## 告警规则

### alerting_rules.yml
```yaml
groups:
  - name: hyperticket_alerts
    interval: 30s
    rules:
      # 错误率告警
      - alert: HighErrorRate
        expr: |
          sum(rate(hyperticket_requests_total{status="failed"}[5m])) 
          / 
          sum(rate(hyperticket_requests_total[5m])) > 0.01
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "High error rate detected"
          description: "Error rate is {{ $value | humanizePercentage }}"
      
      # P99 延迟告警
      - alert: HighLatency
        expr: |
          histogram_quantile(0.99, 
            rate(hyperticket_request_duration_seconds_bucket[5m])
          ) > 1.0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High P99 latency detected"
          description: "P99 latency is {{ $value }}s"
      
      # 数据库连接池耗尽告警
      - alert: DbConnectionPoolExhausted
        expr: hyperticket_db_connections_idle == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Database connection pool exhausted"
          description: "No idle connections available"
      
      # 活跃会话数异常告警
      - alert: AbnormalSessionCount
        expr: hyperticket_sessions_active > 10000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Abnormal session count"
          description: "Active sessions: {{ $value }}"
```

## 性能考虑

### 1. Metrics 开销
- Counter/Gauge：~10ns per operation
- Histogram：~100ns per observation
- 总体开销：< 1% CPU

### 2. 优化建议
- 使用合理的 label 数量（< 10 个）
- 避免高基数 label（如 user_id）
- 使用 Histogram 而非 Summary（更高效）
- 定期清理过期 metrics

## 测试验证

### 单元测试
```cpp
TEST(MetricsManager, RecordRequest)
{
    MetricsManager mgr(8081);
    
    mgr.recordRequest("login", "success");
    mgr.recordRequest("login", "failed");
    
    // 验证 metrics 已记录
    // （需要查询 Prometheus 或使用 registry 的内部 API）
}
```

### 集成测试
```bash
# 发送请求
curl -X POST http://localhost:7000/api/login

# 查询 metrics
curl http://localhost:8080/metrics | grep hyperticket_requests_total

# 预期输出：
# hyperticket_requests_total{method="login",status="success"} 1
```

## 部署建议

### Docker Compose 集成
```yaml
services:
  hyperticket:
    ports:
      - "7000:7000"  # 业务端口
      - "8080:8080"  # Metrics 端口
    environment:
      METRICS_PORT: 8080
```

### Kubernetes 集成
```yaml
apiVersion: v1
kind: Service
metadata:
  name: hyperticket-metrics
  annotations:
    prometheus.io/scrape: "true"
    prometheus.io/port: "8080"
    prometheus.io/path: "/metrics"
spec:
  selector:
    app: hyperticket
  ports:
    - name: metrics
      port: 8080
```

## 预期效果

- ✅ 实时监控系统运行状态
- ✅ P99 延迟 < 100ms 可见
- ✅ 错误率实时告警
- ✅ 容量规划数据支持
- ✅ 性能瓶颈快速定位

## 参考资料

- [prometheus-cpp GitHub](https://github.com/jupp0r/prometheus-cpp)
- [Prometheus 官方文档](https://prometheus.io/docs/)
- [Grafana 仪表盘](https://grafana.com/grafana/dashboards/)
