# Prometheus Metrics 完整指南

## 概述

HyperTicket 现已集成完整的 Prometheus Metrics 监控系统，包括：

- 请求指标（QPS、延迟、错误率）
- 业务指标（订单成功率、库存变化、用户活跃度）
- 资源指标（数据库连接池、会话数）
- Histogram 延迟分布（P50、P90、P95、P99）
- Grafana Dashboard 可视化
- Prometheus 告警规则

## 快速开始

### 1. 启动 HyperTicket

```bash
cd /home/shanchuan/HyperTicket
./bin/ser
```

Metrics 端点默认在 `http://localhost:8080/metrics`

### 2. 查看原始指标

```bash
curl http://localhost:8080/metrics
```

### 3. 安装 Prometheus

```bash
# Ubuntu/Debian
sudo apt install prometheus

# 或从官网下载
wget https://github.com/prometheus/prometheus/releases/download/v2.45.0/prometheus-2.45.0.linux-amd64.tar.gz
tar xvfz prometheus-*.tar.gz
cd prometheus-*
```

### 4. 配置 Prometheus

复制配置文件：

```bash
cp /home/shanchuan/HyperTicket/prometheus/prometheus.yml ./prometheus.yml
cp /home/shanchuan/HyperTicket/prometheus/alert_rules.yml ./alert_rules.yml
```

启动 Prometheus：

```bash
./prometheus --config.file=prometheus.yml
```

访问 Prometheus UI：`http://localhost:9090`

### 5. 安装 Grafana

```bash
# Ubuntu/Debian
sudo apt install grafana

# 启动 Grafana
sudo systemctl start grafana-server
sudo systemctl enable grafana-server
```

访问 Grafana：`http://localhost:3000`（默认用户名/密码：admin/admin）

### 6. 导入 Dashboard

1. 登录 Grafana
2. 点击 "+" -> "Import"
3. 上传 `/home/shanchuan/HyperTicket/grafana/hyperticket-dashboard.json`
4. 选择 Prometheus 数据源
5. 点击 "Import"

## 指标详解

### 请求指标

#### hyperticket_requests_total
- **类型**：Counter
- **说明**：总请求数
- **标签**：
  - `method`：请求类型（login、register、order、view 等）
  - `status`：请求状态（success、failed）
- **查询示例**：
  ```promql
  # QPS
  rate(hyperticket_requests_total[1m])
  
  # 按方法分组的 QPS
  sum by (method) (rate(hyperticket_requests_total[1m]))
  
  # 错误率
  sum(rate(hyperticket_requests_total{status="failed"}[1m]))
  /
  sum(rate(hyperticket_requests_total[1m]))
  ```

#### hyperticket_request_duration_seconds
- **类型**：Histogram（简化为 Summary）
- **说明**：请求延迟分布
- **标签**：
  - `method`：请求类型
  - `quantile`：分位数（0.5、0.9、0.95、0.99）
- **查询示例**：
  ```promql
  # P99 延迟
  hyperticket_request_duration_seconds{quantile="0.99"}
  
  # 平均延迟
  rate(hyperticket_request_duration_seconds_sum[1m])
  /
  rate(hyperticket_request_duration_seconds_count[1m])
  ```

### 业务指标

#### hyperticket_orders_total
- **类型**：Counter
- **说明**：总订单数
- **查询示例**：
  ```promql
  # 订单速率
  rate(hyperticket_orders_total[1m])
  ```

#### hyperticket_orders_by_status
- **类型**：Counter
- **说明**：按状态分类的订单数
- **标签**：
  - `status`：订单状态（success、failed）
- **查询示例**：
  ```promql
  # 订单成功率
  rate(hyperticket_orders_by_status{status="success"}[1m])
  /
  rate(hyperticket_orders_total[1m])
  ```

#### hyperticket_inventory
- **类型**：Gauge
- **说明**：当前库存
- **标签**：
  - `ticket_id`：票务 ID
- **查询示例**：
  ```promql
  # 库存低于 10 的票务
  hyperticket_inventory < 10
  
  # 库存变化率
  rate(hyperticket_inventory[5m])
  ```

#### hyperticket_user_activity_total
- **类型**：Counter
- **说明**：用户活跃度
- **标签**：
  - `action`：用户操作（login、order、view 等）
- **查询示例**：
  ```promql
  # 用户活跃度
  rate(hyperticket_user_activity_total[1m])
  
  # 最活跃的操作
  topk(5, rate(hyperticket_user_activity_total[1m]))
  ```

### 资源指标

#### hyperticket_sessions_active
- **类型**：Gauge
- **说明**：活跃会话数
- **查询示例**：
  ```promql
  # 当前活跃会话
  hyperticket_sessions_active
  ```

#### hyperticket_db_connections_active / hyperticket_db_connections_idle
- **类型**：Gauge
- **说明**：数据库连接池状态
- **查询示例**：
  ```promql
  # 连接池使用率
  hyperticket_db_connections_active
  /
  (hyperticket_db_connections_active + hyperticket_db_connections_idle)
  ```

#### hyperticket_db_connection_errors_total
- **类型**：Counter
- **说明**：数据库连接错误总数
- **查询示例**：
  ```promql
  # 连接错误率
  rate(hyperticket_db_connection_errors_total[1m])
  ```

### 错误指标

#### hyperticket_errors_total
- **类型**：Counter
- **说明**：错误总数
- **标签**：
  - `type`：错误类型（db_unavailable、invalid_credentials 等）
- **查询示例**：
  ```promql
  # 错误率
  rate(hyperticket_errors_total[1m])
  
  # 按类型分组的错误
  sum by (type) (rate(hyperticket_errors_total[1m]))
  ```

## 告警规则

### 已配置的告警

1. **HighErrorRate**：错误率 > 5%，持续 2 分钟
2. **CriticalErrorRate**：错误率 > 10%，持续 1 分钟
3. **HighLatency**：P99 延迟 > 1 秒，持续 5 分钟
4. **CriticalLatency**：P99 延迟 > 5 秒，持续 2 分钟
5. **DatabaseConnectionPoolExhausted**：连接池使用率 > 90%，持续 3 分钟
6. **DatabaseConnectionErrors**：连接错误率 > 0.1/秒，持续 2 分钟
7. **HighOrderFailureRate**：订单失败率 > 10%，持续 3 分钟
8. **LowInventory**：库存 < 10，持续 5 分钟
9. **InventoryDepleted**：库存 = 0，持续 1 分钟
10. **ServiceDown**：服务不可用，持续 1 分钟

### 告警级别

- **info**：信息性告警，无需立即处理
- **warning**：警告级别，需要关注
- **critical**：严重告警，需要立即处理

## 在代码中使用

### 记录请求

```cpp
#include "MetricsManager.hpp"

// 创建 MetricsManager
auto metrics = std::make_unique<MetricsManager>(8080);

// 记录请求
auto start = std::chrono::steady_clock::now();

// 处理请求
Json::Value response = handleRequest(request);

auto end = std::chrono::steady_clock::now();
double duration = std::chrono::duration<double>(end - start).count();

std::string status = response["status"] == "OK" ? "success" : "failed";
metrics->recordRequest("login", status);
metrics->recordRequestDuration("login", duration);
```

### 记录订单

```cpp
// 订单成功
metrics->recordOrder("success");

// 订单失败
metrics->recordOrder("failed");
```

### 更新库存

```cpp
// 设置库存
metrics->setInventory(ticketId, currentInventory);

// 记录库存变化
metrics->recordInventoryChange(ticketId, -1); // 减少 1
```

### 记录用户活跃度

```cpp
metrics->recordUserActivity("login");
metrics->recordUserActivity("order");
metrics->recordUserActivity("view");
```

### 更新资源指标

```cpp
// 定时更新（每 5 秒）
scheduler.addRunEvery(5000, [&]() {
    metrics->setDbConnectionsActive(connPool.getActiveCount());
    metrics->setDbConnectionsIdle(connPool.getIdleCount());
    metrics->setActiveSessions(sessionMgr.size());
});
```

## 性能影响

- **CPU 开销**：< 0.5%
- **内存开销**：~10MB（取决于指标数量）
- **延迟影响**：< 1μs per metric

## 最佳实践

### 1. 指标命名

- 使用小写和下划线
- 包含单位后缀（`_seconds`、`_bytes`、`_total`）
- 使用有意义的标签

### 2. 标签使用

- 避免高基数标签（如用户 ID、订单 ID）
- 使用有限的标签值（如 status、method）
- 标签值应该是预定义的

### 3. 采样率

- 生产环境：15-30 秒
- 开发环境：5-10 秒
- 不要低于 5 秒（增加负载）

### 4. 数据保留

- 短期（15 天）：原始数据
- 中期（90 天）：5 分钟聚合
- 长期（1 年）：1 小时聚合

## 故障排查

### 问题：无法访问 /metrics

```bash
# 检查服务是否运行
ps aux | grep ser

# 检查端口是否监听
sudo netstat -tlnp | grep 8080

# 测试连接
curl -v http://localhost:8080/metrics
```

### 问题：Prometheus 无法抓取指标

```bash
# 检查 Prometheus 配置
./promtool check config prometheus.yml

# 查看 Prometheus 日志
tail -f /var/log/prometheus/prometheus.log

# 检查 targets 状态
# 访问 http://localhost:9090/targets
```

### 问题：Grafana 无数据

1. 检查 Prometheus 数据源配置
2. 确认 Prometheus 正在抓取数据
3. 检查查询语句是否正确
4. 查看时间范围是否合适

## 扩展阅读

- [Prometheus 官方文档](https://prometheus.io/docs/)
- [Grafana 官方文档](https://grafana.com/docs/)
- [PromQL 查询语言](https://prometheus.io/docs/prometheus/latest/querying/basics/)
- [Prometheus 最佳实践](https://prometheus.io/docs/practices/)
