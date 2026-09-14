#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

#include <string>

namespace hyperticket
{
    struct DbConfig
    {
        std::string host = "127.0.0.1";
        int port = 3306;
        std::string user = "root";
        std::string password;
        std::string name = "hyperticket";
        int pool_size = 8;
    };

    struct ServerConfig
    {
        std::string ip = "127.0.0.1";   // 保守默认，仅本地监听；生产部署改为 0.0.0.0
        int port = 7000;
        int io_threads = 1;             // 多 IO 线程竞态问题尚未完全修复，默认保持 1
        int worker_threads = 8;
        int max_connections = 1000;     // 全局最大并发连接数
        int max_requests_per_sec = 20;  // 单连接每秒最大请求数
    };

    struct LogConfig
    {
        std::string basename = "hyperticket";
        size_t roll_size = 1024 * 1024 * 16;
        int flush_interval = 3;
        std::string level = "INFO";
    };

    struct ScheduleConfig
    {
        int stats_interval_ms = 60000;
        int ticket_status_interval_ms = 3600000;
    };

    struct RedisConfig
    {
        std::string host = "127.0.0.1";
        int port = 6379;
        int pool_size = 10;
        int session_ttl_minutes = 30;
        bool enabled = false;  // 默认禁用，使用内存 SessionManager
    };

    struct MetricsConfig
    {
        int port = 8080;
        bool enabled = false;  // 默认禁用
    };

    struct PaymentConfig
    {
        int settle_delay_ms = 1000;      // 发起支付 → 模拟网关结算的延迟
        int settle_interval_ms = 500;    // 定时结算任务扫描间隔
        int success_rate_percent = 100;  // 结算成功率（0-100），<100 用于演练失败路径
    };

    struct AuthConfig
    {
        int max_failures = 5;
        int failure_window_seconds = 900;
        int lock_seconds = 900;
    };

    struct VerificationConfig
    {
        bool email_enabled = false;
        std::string smtp_host;
        int smtp_port = 465;
        std::string smtp_username;
        std::string smtp_auth_code;
        std::string smtp_from;
        std::string smtp_from_name = "HyperTicket";
        bool smtp_use_tls = true;
        bool mock_sms_enabled = false;
        bool expose_mock_sms_code = false;
        int code_ttl_seconds = 300;
        int max_attempts = 5;
        int resend_cooldown_seconds = 60;
        int grant_ttl_seconds = 600;
        bool require_registration_verification = true;
    };

    struct OrderQueueConfig
    {
        bool enabled = true;
        std::string stream = "hyperticket:orders";
        std::string consumer_group = "order-workers";
        std::string consumer_name = "worker-1";
        int poll_interval_ms = 20;
        int batch_size = 32;
        int max_retries = 5;
        int claim_idle_ms = 30000;
    };

    struct AppConfig
    {
        DbConfig db;
        ServerConfig server;
        LogConfig log;
        ScheduleConfig schedule;
        RedisConfig redis;
        MetricsConfig metrics;
        PaymentConfig payment;
        AuthConfig auth;
        VerificationConfig verification;
        OrderQueueConfig order_queue;

        static AppConfig Load(const std::string &path, std::string *error);
    };
} // namespace hyperticket

#endif // APP_CONFIG_HPP
