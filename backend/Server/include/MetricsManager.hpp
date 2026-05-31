#ifndef HYPERTICKET_METRICS_MANAGER_HPP
#define HYPERTICKET_METRICS_MANAGER_HPP

#include <string>
#include <memory>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>

// 注意：此文件为 Prometheus Metrics 监控的接口定义
// 实际实现需要集成 prometheus-cpp 客户端库
// 当前为占位实现，待 Phase 3 完整集成

namespace hyperticket
{
    // Metrics Manager：暴露 Prometheus 格式的监控指标
    //
    // 设计要点：
    // 1. 暴露端口：8080（可配置）
    // 2. 路径：/metrics
    // 3. 格式：Prometheus text format
    // 4. 线程安全：所有方法都是线程安全的
    // 5. 低开销：< 1% CPU
    class MetricsManager
    {
    public:
        // port: Metrics 暴露端口（默认 8080）
        explicit MetricsManager(int port = 8080);
        ~MetricsManager();

        // ============================================================
        // 请求指标（Request Metrics）
        // ============================================================

        // 记录请求：method 为请求类型（login/register/order等），status 为 success/failed
        void recordRequest(const std::string &method, const std::string &status);

        // 记录请求延迟：method 为请求类型，seconds 为耗时（秒）
        void recordRequestDuration(const std::string &method, double seconds);

        // ============================================================
        // 业务指标（Business Metrics）
        // ============================================================

        // 记录订单：status 为 success/failed
        void recordOrder(const std::string &status);

        // 设置活跃订单数
        void setActiveOrders(int count);

        // 设置活跃会话数
        void setActiveSessions(int count);

        // 记录库存变化：ticketId 为票务 ID，delta 为变化量（正数为增加，负数为减少）
        void recordInventoryChange(int ticketId, int delta);

        // 设置当前库存：ticketId 为票务 ID，count 为当前库存
        void setInventory(int ticketId, int count);

        // 记录用户活跃度：action 为用户操作（login/order/view等）
        void recordUserActivity(const std::string &action);

        // ============================================================
        // 资源指标（Resource Metrics）
        // ============================================================

        // 设置数据库活跃连接数
        void setDbConnectionsActive(int count);

        // 设置数据库空闲连接数
        void setDbConnectionsIdle(int count);

        // 记录数据库连接错误
        void recordDbConnectionError();

        // ============================================================
        // 错误指标（Error Metrics）
        // ============================================================

        // 记录错误：type 为错误类型（db_unavailable/invalid_credentials等）
        void recordError(const std::string &type);

        // ============================================================
        // 系统指标（System Metrics）
        // ============================================================

        // 设置内存使用（字节）
        void setMemoryUsage(int64_t bytes);

        // 设置 CPU 使用率（0.0-1.0）
        void setCpuUsage(double ratio);

    private:
        int port_;

        // 简化实现：使用原子变量和 map 存储指标
        // 实际实现时使用 prometheus-cpp 的 Counter/Gauge/Histogram
        std::atomic<int64_t> requestsTotal_{0};
        std::atomic<int64_t> ordersTotal_{0};
        std::atomic<int> activeSessions_{0};
        std::atomic<int> dbConnectionsActive_{0};
        std::atomic<int> dbConnectionsIdle_{0};
        std::atomic<int64_t> dbConnectionErrors_{0};

        std::mutex mutex_;
        std::map<std::string, int64_t> requestsByMethod_;
        std::map<std::string, int64_t> errorsByType_;
        std::map<std::string, int64_t> ordersByStatus_;
        std::map<int, int> inventoryByTicket_;
        std::map<std::string, int64_t> userActivityByAction_;

        // Histogram 数据结构：简化实现，存储延迟样本
        struct HistogramData
        {
            std::vector<double> samples;
            int64_t count{0};
            double sum{0.0};
        };
        std::map<std::string, HistogramData> requestDurationByMethod_;

        // 内部方法
        std::string generateMetrics();
        void runMetricsServer();
        double calculateQuantile(const std::vector<double> &sorted, double quantile);
    };

    // ============================================================
    // 实现说明（待完整集成 prometheus-cpp）
    // ============================================================

    // 依赖安装：
    // Ubuntu: sudo apt install libprometheus-cpp-dev
    // 或从源码编译: https://github.com/jupp0r/prometheus-cpp

    // 示例实现（使用 prometheus-cpp）：
    /*
    #include <prometheus/counter.h>
    #include <prometheus/gauge.h>
    #include <prometheus/histogram.h>
    #include <prometheus/registry.h>
    #include <prometheus/exposer.h>

    class MetricsManager
    {
    public:
        MetricsManager(int port)
            : registry_(std::make_shared<prometheus::Registry>()),
              exposer_(std::make_unique<prometheus::Exposer>("0.0.0.0:" + std::to_string(port))),
              requestsTotal_(prometheus::BuildCounter()
                                .Name("hyperticket_requests_total")
                                .Help("Total number of requests")
                                .Register(*registry_)),
              requestDuration_(prometheus::BuildHistogram()
                                  .Name("hyperticket_request_duration_seconds")
                                  .Help("Request duration in seconds")
                                  .Register(*registry_))
        {
            exposer_->RegisterCollectable(registry_);
            LOG_INFO << "Metrics exposed on port " << port << " at /metrics";
        }

        void recordRequest(const std::string &method, const std::string &status)
        {
            requestsTotal_.Add({{"method", method}, {"status", status}}).Increment();
        }

        void recordRequestDuration(const std::string &method, double seconds)
        {
            static const std::vector<double> buckets = {0.001, 0.01, 0.1, 1.0, 10.0};
            requestDuration_.Add({{"method", method}}, buckets).Observe(seconds);
        }

    private:
        std::shared_ptr<prometheus::Registry> registry_;
        std::unique_ptr<prometheus::Exposer> exposer_;
        prometheus::Family<prometheus::Counter> &requestsTotal_;
        prometheus::Family<prometheus::Histogram> &requestDuration_;
    };
    */

    // ============================================================
    // 使用示例
    // ============================================================

    /*
    // 1. 创建 MetricsManager
    MetricsManager metrics(8080);

    // 2. 在请求处理中记录指标
    auto start = std::chrono::steady_clock::now();
    Json::Value resp = ticketSvc.handle(req);
    auto end = std::chrono::steady_clock::now();

    double duration = std::chrono::duration<double>(end - start).count();
    std::string status = resp["code"].asInt() == 0 ? "success" : "failed";

    metrics.recordRequest("login", status);
    metrics.recordRequestDuration("login", duration);

    // 3. 定时更新资源指标
    scheduler.addRunEvery(5000, [&]() {
        metrics.setDbConnectionsActive(connPool.GetActiveConn());
        metrics.setDbConnectionsIdle(connPool.GetFreeConn());
        metrics.setActiveSessions(sessionMgr.size());
    });

    // 4. 访问 metrics
    // curl http://localhost:8080/metrics
    */

    // ============================================================
    // Prometheus 查询示例
    // ============================================================

    /*
    # QPS
    rate(hyperticket_requests_total[1m])

    # P99 延迟
    histogram_quantile(0.99, rate(hyperticket_request_duration_seconds_bucket[1m]))

    # 错误率
    sum(rate(hyperticket_requests_total{status="failed"}[1m]))
    /
    sum(rate(hyperticket_requests_total[1m]))

    # 数据库连接池使用率
    hyperticket_db_connections_active
    /
    (hyperticket_db_connections_active + hyperticket_db_connections_idle)
    */

} // namespace hyperticket

#endif // HYPERTICKET_METRICS_MANAGER_HPP
