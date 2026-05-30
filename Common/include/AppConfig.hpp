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
        std::string password = "zbk";
        std::string name = "hyperticket";
        int pool_size = 8;
    };

    struct ServerConfig
    {
        std::string ip = "0.0.0.0";
        int port = 7000;
        int io_threads = 4;
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

    struct AppConfig
    {
        DbConfig db;
        ServerConfig server;
        LogConfig log;
        ScheduleConfig schedule;

        static AppConfig Load(const std::string &path, std::string *error);
    };
} // namespace hyperticket

#endif // APP_CONFIG_HPP
