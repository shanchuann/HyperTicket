#include "../include/MetricsManager.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include <sstream>
#include <iomanip>
#include <thread>
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
        // 简化实现：只记录总数，不做 histogram
        // 完整实现需要 prometheus-cpp 的 Histogram 支持
        (void)method;
        (void)seconds;
    }

    void MetricsManager::recordOrder(const std::string &status)
    {
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
