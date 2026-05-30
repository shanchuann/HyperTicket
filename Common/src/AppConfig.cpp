#include "../include/AppConfig.hpp"

#include <fstream>
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
    } // namespace

    AppConfig AppConfig::Load(const std::string &path, std::string *error)
    {
        AppConfig cfg;
        std::ifstream in(path);
        if (!in.good())
        {
            if (error) *error = "config file not found, using defaults";
            return cfg;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::string errs;
        if (!Json::parseFromStream(builder, in, &root, &errs))
        {
            if (error) *error = std::string("config parse failed: ") + errs;
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

        return cfg;
    }
} // namespace hyperticket
