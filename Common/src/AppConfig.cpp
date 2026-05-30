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

        // Overlay DB settings from a .env file located next to the config file.
        // Precedence: real process env > .env file > existing cfg values.
        void applyDbEnv(AppConfig &cfg, const std::string &configPath)
        {
            const auto slash = configPath.find_last_of("/\\");
            const std::string dir = (slash == std::string::npos) ? "" : configPath.substr(0, slash + 1);
            const std::map<std::string, std::string> dotenv = loadDotEnv(dir + ".env");
            cfg.db.host = envOverride(dotenv, "DB_HOST", cfg.db.host);
            cfg.db.port = envOverrideInt(dotenv, "DB_PORT", cfg.db.port);
            cfg.db.user = envOverride(dotenv, "DB_USER", cfg.db.user);
            cfg.db.password = envOverride(dotenv, "DB_PASSWORD", cfg.db.password);
            cfg.db.name = envOverride(dotenv, "DB_NAME", cfg.db.name);
        }
    } // namespace

    AppConfig AppConfig::Load(const std::string &path, std::string *error)
    {
        AppConfig cfg;
        std::ifstream in(path);
        if (!in.good())
        {
            if (error) *error = "config file not found, using defaults";
            applyDbEnv(cfg, path);
            return cfg;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::string errs;
        if (!Json::parseFromStream(builder, in, &root, &errs))
        {
            if (error) *error = std::string("config parse failed: ") + errs;
            applyDbEnv(cfg, path);
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

        // Overlay DB settings from .env (precedence: process env > .env > config.json).
        applyDbEnv(cfg, path);
        return cfg;
    }
} // namespace hyperticket
