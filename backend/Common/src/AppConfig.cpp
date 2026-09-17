#include "../include/AppConfig.hpp"

#include <cstdlib>
#include <fstream>
#include <map>
#include <jsoncpp/json/json.h>

namespace hyperticket
{
    namespace
    {
        std::string getString(const Json::Value &root, const std::string &key, const std::string &fallback)
        {
            return root.isMember(key) ? root[key].asString() : fallback;
        }
        int getInt(const Json::Value &root, const std::string &key, int fallback)
        {
            return root.isMember(key) ? root[key].asInt() : fallback;
        }

        // Trim leading/trailing whitespace.
        std::string trim(const std::string &s)
        {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        // Parse a simple KEY=VALUE .env file. Lines starting with '#' and blank
        // lines are ignored. Surrounding quotes around the value are stripped.
        std::map<std::string, std::string> loadDotEnv(const std::string &path)
        {
            std::map<std::string, std::string> env;
            std::ifstream in(path);
            if (!in.good()) return env;
            std::string line;
            while (std::getline(in, line))
            {
                line = trim(line);
                if (line.empty() || line[0] == '#') continue;
                const auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = trim(line.substr(0, eq));
                std::string val = trim(line.substr(eq + 1));
                if (val.size() >= 2 &&
                    ((val.front() == '"' && val.back() == '"') ||
                     (val.front() == '\'' && val.back() == '\'')))
                {
                    val = val.substr(1, val.size() - 2);
                }
                if (!key.empty()) env[key] = val;
            }
            return env;
        }

        // Resolve a value with precedence: real process env > .env file > current.
        std::string envOverride(const std::map<std::string, std::string> &dotenv,
                                const std::string &key, const std::string &current)
        {
            if (const char *e = std::getenv(key.c_str()); e && *e) return e;
            const auto it = dotenv.find(key);
            if (it != dotenv.end() && !it->second.empty()) return it->second;
            return current;
        }
        int envOverrideInt(const std::map<std::string, std::string> &dotenv,
                           const std::string &key, int current)
        {
            const std::string s = envOverride(dotenv, key, "");
            if (s.empty()) return current;
            try { return std::stoi(s); } catch (...) { return current; }
        }
        bool envOverrideBool(const std::map<std::string, std::string> &dotenv,
                             const std::string &key, bool current)
        {
            const std::string value = envOverride(dotenv, key, "");
            if (value == "1" || value == "true" || value == "TRUE") return true;
            if (value == "0" || value == "false" || value == "FALSE") return false;
            return current;
        }

        // Overlay deployable settings from a .env file located next to the config file.
        // Precedence: real process env > .env file > existing cfg values.
        void applyEnv(AppConfig &cfg, const std::string &configPath)
        {
            const auto slash = configPath.find_last_of("/\\");
            const std::string dir = (slash == std::string::npos) ? "" : configPath.substr(0, slash + 1);
            const std::map<std::string, std::string> dotenv = loadDotEnv(dir + ".env");
            cfg.db.host = envOverride(dotenv, "DB_HOST", cfg.db.host);
            cfg.db.port = envOverrideInt(dotenv, "DB_PORT", cfg.db.port);
            cfg.db.user = envOverride(dotenv, "DB_USER", cfg.db.user);
            cfg.db.password = envOverride(dotenv, "DB_PASSWORD", cfg.db.password);
            cfg.db.name = envOverride(dotenv, "DB_NAME", cfg.db.name);
            cfg.db.pool_size = envOverrideInt(dotenv, "DB_POOL_SIZE", cfg.db.pool_size);
            cfg.server.ip = envOverride(dotenv, "SERVER_IP", cfg.server.ip);
            cfg.server.port = envOverrideInt(dotenv, "SERVER_PORT", cfg.server.port);
            cfg.server.io_threads = envOverrideInt(dotenv, "SERVER_IO_THREADS", cfg.server.io_threads);
            cfg.server.worker_threads = envOverrideInt(dotenv, "SERVER_WORKER_THREADS", cfg.server.worker_threads);
            cfg.server.max_connections = envOverrideInt(dotenv, "SERVER_MAX_CONNECTIONS", cfg.server.max_connections);
            cfg.server.max_requests_per_sec = envOverrideInt(
                dotenv, "SERVER_MAX_REQUESTS_PER_SEC", cfg.server.max_requests_per_sec);
            cfg.server.max_request_bytes = envOverrideInt(
                dotenv, "SERVER_MAX_REQUEST_BYTES", cfg.server.max_request_bytes);
            cfg.server.gateway_token = envOverride(
                dotenv, "HYPERTICKET_GATEWAY_TOKEN", cfg.server.gateway_token);
            cfg.redis.host = envOverride(dotenv, "REDIS_HOST", cfg.redis.host);
            cfg.redis.port = envOverrideInt(dotenv, "REDIS_PORT", cfg.redis.port);
            cfg.redis.pool_size = envOverrideInt(dotenv, "REDIS_POOL_SIZE", cfg.redis.pool_size);
            cfg.redis.session_ttl_minutes = envOverrideInt(
                dotenv, "REDIS_SESSION_TTL_MINUTES", cfg.redis.session_ttl_minutes);
            cfg.redis.enabled = envOverrideBool(dotenv, "REDIS_ENABLED", cfg.redis.enabled);
            cfg.metrics.port = envOverrideInt(dotenv, "METRICS_PORT", cfg.metrics.port);
            cfg.metrics.enabled = envOverrideBool(dotenv, "METRICS_ENABLED", cfg.metrics.enabled);
            cfg.verification.smtp_host = envOverride(dotenv, "HYPERTICKET_SMTP_HOST", cfg.verification.smtp_host);
            cfg.verification.smtp_port = envOverrideInt(dotenv, "HYPERTICKET_SMTP_PORT", cfg.verification.smtp_port);
            cfg.verification.smtp_username = envOverride(dotenv, "HYPERTICKET_SMTP_USERNAME", cfg.verification.smtp_username);
            cfg.verification.smtp_auth_code = envOverride(dotenv, "HYPERTICKET_SMTP_AUTH_CODE", cfg.verification.smtp_auth_code);
            cfg.verification.smtp_from = envOverride(dotenv, "HYPERTICKET_SMTP_FROM", cfg.verification.smtp_from);
            cfg.verification.smtp_from_name = envOverride(dotenv, "HYPERTICKET_SMTP_FROM_NAME", cfg.verification.smtp_from_name);
            cfg.verification.smtp_use_tls = envOverrideBool(dotenv, "HYPERTICKET_SMTP_USE_TLS", cfg.verification.smtp_use_tls);
            cfg.verification.development_inbox_enabled = envOverrideBool(
                dotenv, "HYPERTICKET_VERIFICATION_DEV_INBOX_ENABLED",
                cfg.verification.development_inbox_enabled);
            cfg.verification.development_inbox_path = envOverride(
                dotenv, "HYPERTICKET_VERIFICATION_DEV_INBOX_PATH",
                cfg.verification.development_inbox_path);
            cfg.verification.mock_sms_enabled = envOverrideBool(
                dotenv, "HYPERTICKET_VERIFICATION_MOCK_SMS_ENABLED",
                cfg.verification.mock_sms_enabled);
            cfg.verification.expose_mock_sms_code = envOverrideBool(
                dotenv, "HYPERTICKET_VERIFICATION_EXPOSE_MOCK_CODE",
                cfg.verification.expose_mock_sms_code);
            cfg.verification.require_registration_verification = envOverrideBool(
                dotenv, "HYPERTICKET_VERIFICATION_REQUIRED",
                cfg.verification.require_registration_verification);
            cfg.payment.simulated_channels_enabled = envOverrideBool(
                dotenv, "HYPERTICKET_PAYMENT_SIMULATED_CHANNELS_ENABLED",
                cfg.payment.simulated_channels_enabled);
            cfg.payment.simulated_webhook_secret = envOverride(
                dotenv, "HYPERTICKET_PAYMENT_SIMULATED_WEBHOOK_SECRET",
                cfg.payment.simulated_webhook_secret);
            cfg.verification.email_enabled = !cfg.verification.smtp_host.empty() &&
                                             !cfg.verification.smtp_username.empty() &&
                                             !cfg.verification.smtp_auth_code.empty();
        }
    } // namespace

    AppConfig AppConfig::Load(const std::string &path, std::string *error)
    {
        AppConfig cfg;
        std::ifstream in(path);
        if (!in.good())
        {
            if (error) *error = "config file not found, using defaults";
            applyEnv(cfg, path);
            return cfg;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::string errs;
        if (!Json::parseFromStream(builder, in, &root, &errs))
        {
            if (error) *error = std::string("config parse failed: ") + errs;
            applyEnv(cfg, path);
            return cfg;
        }

        if (root.isMember("db"))
        {
            const Json::Value &db = root["db"];
            cfg.db.host = getString(db, "host", cfg.db.host);
            cfg.db.port = getInt(db, "port", cfg.db.port);
            cfg.db.user = getString(db, "user", cfg.db.user);
            cfg.db.password = getString(db, "password", cfg.db.password);
            cfg.db.name = getString(db, "name", cfg.db.name);
            cfg.db.pool_size = getInt(db, "pool_size", cfg.db.pool_size);
        }

        if (root.isMember("server"))
        {
            const Json::Value &server = root["server"];
            cfg.server.ip = getString(server, "ip", cfg.server.ip);
            cfg.server.port = getInt(server, "port", cfg.server.port);
            cfg.server.io_threads = getInt(server, "io_threads", cfg.server.io_threads);
            cfg.server.worker_threads = getInt(server, "worker_threads", cfg.server.worker_threads);
            cfg.server.max_connections = getInt(server, "max_connections", cfg.server.max_connections);
            cfg.server.max_requests_per_sec = getInt(server, "max_requests_per_sec", cfg.server.max_requests_per_sec);
            cfg.server.max_request_bytes = getInt(server, "max_request_bytes", cfg.server.max_request_bytes);
            cfg.server.gateway_token = getString(server, "gateway_token", cfg.server.gateway_token);
        }

        if (root.isMember("log"))
        {
            const Json::Value &log = root["log"];
            cfg.log.basename = getString(log, "basename", cfg.log.basename);
            cfg.log.roll_size = static_cast<size_t>(getInt(log, "roll_size", static_cast<int>(cfg.log.roll_size)));
            cfg.log.flush_interval = getInt(log, "flush_interval", cfg.log.flush_interval);
            cfg.log.level = getString(log, "level", cfg.log.level);
        }

        if (root.isMember("schedule"))
        {
            const Json::Value &schedule = root["schedule"];
            cfg.schedule.stats_interval_ms = getInt(schedule, "stats_interval_ms", cfg.schedule.stats_interval_ms);
            cfg.schedule.ticket_status_interval_ms = getInt(schedule, "ticket_status_interval_ms", cfg.schedule.ticket_status_interval_ms);
        }

        if (root.isMember("redis"))
        {
            const Json::Value &redis = root["redis"];
            cfg.redis.host = getString(redis, "host", cfg.redis.host);
            cfg.redis.port = getInt(redis, "port", cfg.redis.port);
            cfg.redis.pool_size = getInt(redis, "pool_size", cfg.redis.pool_size);
            cfg.redis.session_ttl_minutes = getInt(redis, "session_ttl_minutes", cfg.redis.session_ttl_minutes);
            cfg.redis.enabled = root["redis"].isMember("enabled") ? root["redis"]["enabled"].asBool() : cfg.redis.enabled;
        }

        if (root.isMember("metrics"))
        {
            const Json::Value &metrics = root["metrics"];
            cfg.metrics.port = getInt(metrics, "port", cfg.metrics.port);
            cfg.metrics.enabled = root["metrics"].isMember("enabled") ? root["metrics"]["enabled"].asBool() : cfg.metrics.enabled;
        }

        if (root.isMember("payment"))
        {
            const Json::Value &payment = root["payment"];
            cfg.payment.settle_delay_ms = getInt(payment, "settle_delay_ms", cfg.payment.settle_delay_ms);
            cfg.payment.settle_interval_ms = getInt(payment, "settle_interval_ms", cfg.payment.settle_interval_ms);
            cfg.payment.success_rate_percent = getInt(payment, "success_rate_percent", cfg.payment.success_rate_percent);
            cfg.payment.simulated_channels_enabled = payment.get(
                "simulated_channels_enabled", cfg.payment.simulated_channels_enabled).asBool();
        }

        if (root.isMember("auth"))
        {
            const Json::Value &auth = root["auth"];
            cfg.auth.max_failures = getInt(auth, "max_failures", cfg.auth.max_failures);
            cfg.auth.failure_window_seconds = getInt(auth, "failure_window_seconds", cfg.auth.failure_window_seconds);
            cfg.auth.lock_seconds = getInt(auth, "lock_seconds", cfg.auth.lock_seconds);
        }

        if (root.isMember("verification"))
        {
            const Json::Value &verification = root["verification"];
            cfg.verification.mock_sms_enabled = verification.get("mock_sms_enabled", cfg.verification.mock_sms_enabled).asBool();
            cfg.verification.expose_mock_sms_code = verification.get("expose_mock_sms_code", cfg.verification.expose_mock_sms_code).asBool();
            cfg.verification.development_inbox_enabled = verification.get("development_inbox_enabled", cfg.verification.development_inbox_enabled).asBool();
            cfg.verification.development_inbox_path = getString(verification, "development_inbox_path", cfg.verification.development_inbox_path);
            cfg.verification.code_ttl_seconds = getInt(verification, "code_ttl_seconds", cfg.verification.code_ttl_seconds);
            cfg.verification.max_attempts = getInt(verification, "max_attempts", cfg.verification.max_attempts);
            cfg.verification.resend_cooldown_seconds = getInt(verification, "resend_cooldown_seconds", cfg.verification.resend_cooldown_seconds);
            cfg.verification.daily_send_limit = getInt(verification, "daily_send_limit", cfg.verification.daily_send_limit);
            cfg.verification.grant_ttl_seconds = getInt(verification, "grant_ttl_seconds", cfg.verification.grant_ttl_seconds);
            cfg.verification.require_registration_verification = verification.get("require_registration_verification", cfg.verification.require_registration_verification).asBool();
        }

        if (root.isMember("order_queue"))
        {
            const Json::Value &queue = root["order_queue"];
            cfg.order_queue.enabled = queue.isMember("enabled") ? queue["enabled"].asBool() : cfg.order_queue.enabled;
            cfg.order_queue.stream = getString(queue, "stream", cfg.order_queue.stream);
            cfg.order_queue.consumer_group = getString(queue, "consumer_group", cfg.order_queue.consumer_group);
            cfg.order_queue.consumer_name = getString(queue, "consumer_name", cfg.order_queue.consumer_name);
            cfg.order_queue.poll_interval_ms = getInt(queue, "poll_interval_ms", cfg.order_queue.poll_interval_ms);
            cfg.order_queue.batch_size = getInt(queue, "batch_size", cfg.order_queue.batch_size);
            cfg.order_queue.max_retries = getInt(queue, "max_retries", cfg.order_queue.max_retries);
            cfg.order_queue.claim_idle_ms = getInt(queue, "claim_idle_ms", cfg.order_queue.claim_idle_ms);
        }

        // Overlay DB settings from .env (precedence: process env > .env > config.json).
        applyEnv(cfg, path);
        return cfg;
    }
} // namespace hyperticket
