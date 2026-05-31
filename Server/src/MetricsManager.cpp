#include "../include/MetricsManager.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

namespace hyperticket
{
    MetricsManager::MetricsManager(int port)
        : port_(port)
    {
        // 启动一个简单的 HTTP 服务器来暴露 metrics
        std::thread([this]() {
            this->runMetricsServer();
        }).detach();

        LOG_INFO << "Metrics server started on port " << port_ << " at /metrics";
    }

    MetricsManager::~MetricsManager()
    {
        // 清理资源
    }

    void MetricsManager::recordRequest(const std::string &method, const std::string &status)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = method + ":" + status;
        requestsByMethod_[key]++;
        requestsTotal_++;
    }

    void MetricsManager::recordRequestDuration(const std::string &method, double seconds)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &hist = requestDurationByMethod_[method];
        hist.samples.push_back(seconds);
        hist.count++;
        hist.sum += seconds;

        // 限制样本数量，避免内存无限增长
        if (hist.samples.size() > 10000)
        {
            hist.samples.erase(hist.samples.begin(), hist.samples.begin() + 5000);
        }
    }

    void MetricsManager::recordOrder(const std::string &status)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ordersByStatus_[status]++;
        ordersTotal_++;
    }

    void MetricsManager::setActiveOrders(int count)
    {
        // 简化实现：暂不支持
        (void)count;
    }

    void MetricsManager::setActiveSessions(int count)
    {
        activeSessions_.store(count);
    }

    void MetricsManager::setDbConnectionsActive(int count)
    {
        dbConnectionsActive_.store(count);
    }

    void MetricsManager::setDbConnectionsIdle(int count)
    {
        dbConnectionsIdle_.store(count);
    }

    void MetricsManager::recordDbConnectionError()
    {
        dbConnectionErrors_++;
    }

    void MetricsManager::recordError(const std::string &type)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        errorsByType_[type]++;
    }

    void MetricsManager::setMemoryUsage(int64_t bytes)
    {
        (void)bytes;
    }

    void MetricsManager::setCpuUsage(double ratio)
    {
        (void)ratio;
    }

    void MetricsManager::recordInventoryChange(int ticketId, int delta)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        inventoryByTicket_[ticketId] += delta;
    }

    void MetricsManager::setInventory(int ticketId, int count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        inventoryByTicket_[ticketId] = count;
    }

    void MetricsManager::recordUserActivity(const std::string &action)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        userActivityByAction_[action]++;
    }

    double MetricsManager::calculateQuantile(const std::vector<double> &sorted, double quantile)
    {
        if (sorted.empty())
        {
            return 0.0;
        }

        double index = quantile * (sorted.size() - 1);
        size_t lower = static_cast<size_t>(index);
        size_t upper = lower + 1;

        if (upper >= sorted.size())
        {
            return sorted.back();
        }

        double weight = index - lower;
        return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    }

    std::string MetricsManager::generateMetrics()
    {
        std::ostringstream oss;

        // Prometheus text format
        oss << "# HELP hyperticket_requests_total Total number of requests\n";
        oss << "# TYPE hyperticket_requests_total counter\n";

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &pair : requestsByMethod_)
            {
                size_t pos = pair.first.find(':');
                if (pos != std::string::npos)
                {
                    std::string method = pair.first.substr(0, pos);
                    std::string status = pair.first.substr(pos + 1);
                    oss << "hyperticket_requests_total{method=\"" << method
                        << "\",status=\"" << status << "\"} " << pair.second << "\n";
                }
            }
        }

        oss << "\n# HELP hyperticket_orders_total Total number of orders\n";
        oss << "# TYPE hyperticket_orders_total counter\n";
        oss << "hyperticket_orders_total " << ordersTotal_.load() << "\n";

        oss << "\n# HELP hyperticket_orders_by_status Total number of orders by status\n";
        oss << "# TYPE hyperticket_orders_by_status counter\n";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &pair : ordersByStatus_)
            {
                oss << "hyperticket_orders_by_status{status=\"" << pair.first << "\"} "
                    << pair.second << "\n";
            }
        }

        oss << "\n# HELP hyperticket_inventory Current inventory by ticket\n";
        oss << "# TYPE hyperticket_inventory gauge\n";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &pair : inventoryByTicket_)
            {
                oss << "hyperticket_inventory{ticket_id=\"" << pair.first << "\"} "
                    << pair.second << "\n";
            }
        }

        oss << "\n# HELP hyperticket_user_activity_total Total user activity by action\n";
        oss << "# TYPE hyperticket_user_activity_total counter\n";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &pair : userActivityByAction_)
            {
                oss << "hyperticket_user_activity_total{action=\"" << pair.first << "\"} "
                    << pair.second << "\n";
            }
        }

        // Histogram 指标：请求延迟
        oss << "\n# HELP hyperticket_request_duration_seconds Request duration in seconds\n";
        oss << "# TYPE hyperticket_request_duration_seconds histogram\n";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &pair : requestDurationByMethod_)
            {
                const std::string &method = pair.first;
                const HistogramData &hist = pair.second;

                if (hist.samples.empty())
                {
                    continue;
                }

                // 计算分位数
                std::vector<double> sorted = hist.samples;
                std::sort(sorted.begin(), sorted.end());

                double p50 = calculateQuantile(sorted, 0.50);
                double p90 = calculateQuantile(sorted, 0.90);
                double p95 = calculateQuantile(sorted, 0.95);
                double p99 = calculateQuantile(sorted, 0.99);

                // Prometheus histogram format (simplified)
                // 实际应该输出 bucket，这里简化为 summary 格式
                oss << "hyperticket_request_duration_seconds{method=\"" << method
                    << "\",quantile=\"0.5\"} " << std::fixed << std::setprecision(6) << p50 << "\n";
                oss << "hyperticket_request_duration_seconds{method=\"" << method
                    << "\",quantile=\"0.9\"} " << p90 << "\n";
                oss << "hyperticket_request_duration_seconds{method=\"" << method
                    << "\",quantile=\"0.95\"} " << p95 << "\n";
                oss << "hyperticket_request_duration_seconds{method=\"" << method
                    << "\",quantile=\"0.99\"} " << p99 << "\n";
                oss << "hyperticket_request_duration_seconds_sum{method=\"" << method
                    << "\"} " << hist.sum << "\n";
                oss << "hyperticket_request_duration_seconds_count{method=\"" << method
                    << "\"} " << hist.count << "\n";
            }
        }

        oss << "\n# HELP hyperticket_sessions_active Number of active sessions\n";
        oss << "# TYPE hyperticket_sessions_active gauge\n";
        oss << "hyperticket_sessions_active " << activeSessions_.load() << "\n";

        oss << "\n# HELP hyperticket_db_connections_active Number of active database connections\n";
        oss << "# TYPE hyperticket_db_connections_active gauge\n";
        oss << "hyperticket_db_connections_active " << dbConnectionsActive_.load() << "\n";

        oss << "\n# HELP hyperticket_db_connections_idle Number of idle database connections\n";
        oss << "# TYPE hyperticket_db_connections_idle gauge\n";
        oss << "hyperticket_db_connections_idle " << dbConnectionsIdle_.load() << "\n";

        oss << "\n# HELP hyperticket_db_connection_errors_total Total number of database connection errors\n";
        oss << "# TYPE hyperticket_db_connection_errors_total counter\n";
        oss << "hyperticket_db_connection_errors_total " << dbConnectionErrors_.load() << "\n";

        oss << "\n# HELP hyperticket_errors_total Total number of errors by type\n";
        oss << "# TYPE hyperticket_errors_total counter\n";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &pair : errorsByType_)
            {
                oss << "hyperticket_errors_total{type=\"" << pair.first << "\"} "
                    << pair.second << "\n";
            }
        }

        return oss.str();
    }

    void MetricsManager::runMetricsServer()
    {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            LOG_ERROR << "Failed to create metrics socket";
            return;
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            LOG_ERROR << "Failed to bind metrics socket on port " << port_;
            close(server_fd);
            return;
        }

        if (listen(server_fd, 10) < 0)
        {
            LOG_ERROR << "Failed to listen on metrics socket";
            close(server_fd);
            return;
        }

        while (true)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

            if (client_fd < 0)
            {
                continue;
            }

            // 读取 HTTP 请求（简化处理）
            char buffer[1024] = {0};
            read(client_fd, buffer, sizeof(buffer) - 1);

            // 检查是否是 GET /metrics
            if (strstr(buffer, "GET /metrics") != nullptr)
            {
                std::string metrics = generateMetrics();

                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n";
                response << "Content-Type: text/plain; version=0.0.4\r\n";
                response << "Content-Length: " << metrics.length() << "\r\n";
                response << "Connection: close\r\n";
                response << "\r\n";
                response << metrics;

                std::string resp_str = response.str();
                write(client_fd, resp_str.c_str(), resp_str.length());
            }
            else
            {
                const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                write(client_fd, not_found, strlen(not_found));
            }

            close(client_fd);
        }

        close(server_fd);
    }

} // namespace hyperticket
