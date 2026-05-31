#ifndef HYPERTICKET_HEALTH_CHECK_HPP
#define HYPERTICKET_HEALTH_CHECK_HPP

#include <string>
#include <chrono>
#include <mysql/mysql.h>
#include "../../Common/include/Protocol.hpp"

namespace hyperticket
{
    // 健康检查状态
    enum class HealthStatus
    {
        Healthy,    // 所有检查通过
        Degraded,   // 部分检查失败，但核心功能可用
        Unhealthy   // 关键检查失败，服务不可用
    };

    // 单个检查项的结果
    struct CheckResult
    {
        std::string name;
        bool passed;
        std::string message;
        int64_t durationMs;
    };

    // 健康检查响应
    struct HealthCheckResponse
    {
        HealthStatus status;
        std::vector<CheckResult> checks;
        int64_t uptimeSeconds;
        std::string version;
        int64_t timestamp;
    };

    // 健康检查器：检查各个组件的健康状态
    class HealthChecker
    {
    public:
        HealthChecker(MYSQL *dbConn, const std::string &version)
            : dbConn_(dbConn), version_(version), startTime_(std::chrono::steady_clock::now()) {}

        // 执行所有健康检查
        HealthCheckResponse check();

        // 转换为 JSON 响应
        Json::Value toJson(const HealthCheckResponse &resp);

    private:
        // 各个组件的健康检查
        CheckResult checkDatabase();
        CheckResult checkDiskSpace();
        CheckResult checkMemory();
        // CheckResult checkRedis();  // 待 Redis 集成后启用

        // 辅助方法
        int64_t getUptimeSeconds() const;
        int64_t getCurrentTimestamp() const;

        MYSQL *dbConn_;
        std::string version_;
        std::chrono::steady_clock::time_point startTime_;
    };

    // 实现

    inline HealthCheckResponse HealthChecker::check()
    {
        HealthCheckResponse resp;
        resp.version = version_;
        resp.uptimeSeconds = getUptimeSeconds();
        resp.timestamp = getCurrentTimestamp();

        // 执行各项检查
        resp.checks.push_back(checkDatabase());
        resp.checks.push_back(checkDiskSpace());
        resp.checks.push_back(checkMemory());

        // 确定整体状态
        int failedCount = 0;
        int criticalFailedCount = 0;

        for (const auto &check : resp.checks)
        {
            if (!check.passed)
            {
                ++failedCount;
                // 数据库检查失败视为关键失败
                if (check.name == "database")
                {
                    ++criticalFailedCount;
                }
            }
        }

        if (criticalFailedCount > 0)
        {
            resp.status = HealthStatus::Unhealthy;
        }
        else if (failedCount > 0)
        {
            resp.status = HealthStatus::Degraded;
        }
        else
        {
            resp.status = HealthStatus::Healthy;
        }

        return resp;
    }

    inline CheckResult HealthChecker::checkDatabase()
    {
        auto start = std::chrono::steady_clock::now();
        CheckResult result;
        result.name = "database";

        if (!dbConn_)
        {
            result.passed = false;
            result.message = "Database connection is null";
            result.durationMs = 0;
            return result;
        }

        // 执行简单的 ping 检查
        int pingResult = mysql_ping(dbConn_);
        auto end = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (pingResult == 0)
        {
            result.passed = true;
            result.message = "ok";
        }
        else
        {
            result.passed = false;
            result.message = std::string("ping failed: ") + mysql_error(dbConn_);
        }

        return result;
    }

    inline CheckResult HealthChecker::checkDiskSpace()
    {
        auto start = std::chrono::steady_clock::now();
        CheckResult result;
        result.name = "disk_space";

        // 检查当前目录的磁盘空间
        FILE *fp = popen("df -h . | tail -1 | awk '{print $5}' | sed 's/%//'", "r");
        if (!fp)
        {
            result.passed = false;
            result.message = "Failed to check disk space";
            result.durationMs = 0;
            return result;
        }

        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp) != nullptr)
        {
            int usage = atoi(buffer);
            pclose(fp);

            auto end = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            if (usage < 90)
            {
                result.passed = true;
                result.message = std::string("usage: ") + std::to_string(usage) + "%";
            }
            else
            {
                result.passed = false;
                result.message = std::string("disk usage too high: ") + std::to_string(usage) + "%";
            }
        }
        else
        {
            pclose(fp);
            result.passed = false;
            result.message = "Failed to read disk usage";
            result.durationMs = 0;
        }

        return result;
    }

    inline CheckResult HealthChecker::checkMemory()
    {
        auto start = std::chrono::steady_clock::now();
        CheckResult result;
        result.name = "memory";

        // 检查内存使用率
        FILE *fp = popen("free | grep Mem | awk '{printf \"%.0f\", $3/$2 * 100.0}'", "r");
        if (!fp)
        {
            result.passed = false;
            result.message = "Failed to check memory";
            result.durationMs = 0;
            return result;
        }

        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp) != nullptr)
        {
            int usage = atoi(buffer);
            pclose(fp);

            auto end = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            if (usage < 90)
            {
                result.passed = true;
                result.message = std::string("usage: ") + std::to_string(usage) + "%";
            }
            else
            {
                result.passed = false;
                result.message = std::string("memory usage too high: ") + std::to_string(usage) + "%";
            }
        }
        else
        {
            pclose(fp);
            result.passed = false;
            result.message = "Failed to read memory usage";
            result.durationMs = 0;
        }

        return result;
    }

    inline int64_t HealthChecker::getUptimeSeconds() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();
    }

    inline int64_t HealthChecker::getCurrentTimestamp() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    inline Json::Value HealthChecker::toJson(const HealthCheckResponse &resp)
    {
        Json::Value root;

        // 状态
        switch (resp.status)
        {
        case HealthStatus::Healthy:
            root["status"] = "healthy";
            break;
        case HealthStatus::Degraded:
            root["status"] = "degraded";
            break;
        case HealthStatus::Unhealthy:
            root["status"] = "unhealthy";
            break;
        }

        // 检查项
        Json::Value checks;
        for (const auto &check : resp.checks)
        {
            Json::Value item;
            item["passed"] = check.passed;
            item["message"] = check.message;
            item["duration_ms"] = static_cast<int>(check.durationMs);
            checks[check.name] = item;
        }
        root["checks"] = checks;

        // 元信息
        root["uptime_seconds"] = static_cast<Json::Int64>(resp.uptimeSeconds);
        root["version"] = resp.version;
        root["timestamp"] = static_cast<Json::Int64>(resp.timestamp);

        return root;
    }

} // namespace hyperticket

#endif // HYPERTICKET_HEALTH_CHECK_HPP
