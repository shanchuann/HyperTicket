#include "../include/RedisSessionManager.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include <jsoncpp/json/json.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>

// 注意：此实现为占位版本，使用文件模拟 Redis
// 生产环境请安装 hiredis 或 redis-plus-plus 并使用真实 Redis

namespace hyperticket
{
    RedisSessionManager::RedisSessionManager(const std::string &redisHost,
                                             int redisPort,
                                             int64_t ttlMs)
        : redisHost_(redisHost), redisPort_(redisPort), ttlMs_(ttlMs)
    {
        // 占位实现：记录配置但不实际连接 Redis
        LOG_WARN << "RedisSessionManager: Using placeholder implementation (file-based)";
        LOG_WARN << "For production, install hiredis and rebuild with Redis support";
        LOG_INFO << "Redis config: " << redisHost << ":" << redisPort << ", TTL=" << ttlMs / 1000 << "s";
    }

    RedisSessionManager::~RedisSessionManager()
    {
        // 清理资源
    }

    std::string RedisSessionManager::generateToken()
    {
        // 生成 32 字节随机 token（64 个 hex 字符）
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;

        std::ostringstream oss;
        for (int i = 0; i < 4; ++i)
        {
            uint64_t val = dis(gen);
            oss << std::hex << std::setfill('0') << std::setw(16) << val;
        }
        return oss.str();
    }

    std::string RedisSessionManager::create(const std::string &tel, int64_t userId, int64_t nowMs)
    {
        std::string token = generateToken();

        // 构造 JSON value
        Json::Value value;
        value["tel"] = tel;
        value["userId"] = static_cast<Json::Int64>(userId);
        value["expireMs"] = static_cast<Json::Int64>(nowMs + ttlMs_);

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string jsonStr = Json::writeString(builder, value);

        // 占位实现：写入文件而非 Redis
        // 生产环境应使用：redisCommand(ctx, "SETEX session:%s %d %s", token, ttl, json)
        std::string filename = "/tmp/hyperticket_session_" + token;
        std::ofstream ofs(filename);
        if (ofs.is_open())
        {
            ofs << jsonStr;
            ofs.close();
        }

        LOG_DEBUG << "Session created: " << token.substr(0, 8) << "... for user " << userId;
        return token;
    }

    bool RedisSessionManager::resolve(const std::string &token, int64_t nowMs,
                                      std::string &telOut, int64_t &userIdOut)
    {
        // 占位实现：从文件读取而非 Redis
        // 生产环境应使用：redisCommand(ctx, "GET session:%s", token)
        std::string filename = "/tmp/hyperticket_session_" + token;
        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            return false;
        }

        std::string jsonStr((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        ifs.close();

        // 解析 JSON
        Json::CharReaderBuilder builder;
        Json::Value value;
        std::istringstream iss(jsonStr);
        std::string errs;
        if (!Json::parseFromStream(builder, iss, &value, &errs))
        {
            LOG_ERROR << "JSON parse failed: " << errs;
            return false;
        }

        // 检查过期
        int64_t expireMs = value["expireMs"].asInt64();
        if (expireMs <= nowMs)
        {
            remove(token);
            return false;
        }

        telOut = value["tel"].asString();
        userIdOut = value["userId"].asInt64();

        // 续期：更新过期时间
        value["expireMs"] = static_cast<Json::Int64>(nowMs + ttlMs_);
        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        std::string newJsonStr = Json::writeString(wbuilder, value);

        std::ofstream ofs(filename);
        if (ofs.is_open())
        {
            ofs << newJsonStr;
            ofs.close();
        }

        return true;
    }

    void RedisSessionManager::purgeExpired(int64_t nowMs)
    {
        // Redis 自动过期，无需手动清理
        // 占位实现也不需要清理（文件会在 resolve 时检查过期）
        (void)nowMs;
    }

    void RedisSessionManager::remove(const std::string &token)
    {
        // 占位实现：删除文件
        // 生产环境应使用：redisCommand(ctx, "DEL session:%s", token)
        std::string filename = "/tmp/hyperticket_session_" + token;
        std::remove(filename.c_str());
        LOG_DEBUG << "Session removed: " << token.substr(0, 8) << "...";
    }

    bool RedisSessionManager::ping()
    {
        // 占位实现：总是返回 true
        // 生产环境应使用：redisCommand(ctx, "PING")
        return true;
    }

} // namespace hyperticket

/*
 * ============================================================
 * 生产环境实现（使用 hiredis）
 * ============================================================
 *
 * 依赖安装：
 *   sudo apt install libhiredis-dev
 *
 * CMakeLists.txt 添加：
 *   find_library(HIREDIS_LIB hiredis)
 *   target_link_libraries(ser PRIVATE ${HIREDIS_LIB})
 *
 * 完整实现示例：
 *
 * #include <hiredis/hiredis.h>
 *
 * class RedisSessionManager : public ISessionManager
 * {
 * private:
 *     redisContext *ctx_;
 *
 * public:
 *     RedisSessionManager(const std::string &host, int port, int64_t ttl)
 *         : redisHost_(host), redisPort_(port), ttlMs_(ttl)
 *     {
 *         ctx_ = redisConnect(host.c_str(), port);
 *         if (!ctx_ || ctx_->err) {
 *             LOG_FATAL << "Redis connect failed: " << (ctx_ ? ctx_->errstr : "null context");
 *             throw std::runtime_error("Redis connection failed");
 *         }
 *         LOG_INFO << "Redis connected: " << host << ":" << port;
 *     }
 *
 *     ~RedisSessionManager()
 *     {
 *         if (ctx_) {
 *             redisFree(ctx_);
 *         }
 *     }
 *
 *     std::string create(const std::string &tel, int64_t userId, int64_t nowMs) override
 *     {
 *         std::string token = generateToken();
 *         Json::Value value;
 *         value["tel"] = tel;
 *         value["userId"] = static_cast<Json::Int64>(userId);
 *         value["expireMs"] = static_cast<Json::Int64>(nowMs + ttlMs_);
 *
 *         Json::StreamWriterBuilder builder;
 *         builder["indentation"] = "";
 *         std::string jsonStr = Json::writeString(builder, value);
 *
 *         std::string key = "session:" + token;
 *         int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
 *
 *         redisReply *reply = (redisReply*)redisCommand(ctx_,
 *             "SETEX %s %d %s", key.c_str(), ttlSeconds, jsonStr.c_str());
 *
 *         if (!reply || reply->type == REDIS_REPLY_ERROR) {
 *             LOG_ERROR << "Redis SETEX failed";
 *             if (reply) freeReplyObject(reply);
 *             return "";
 *         }
 *
 *         freeReplyObject(reply);
 *         return token;
 *     }
 *
 *     bool resolve(const std::string &token, int64_t nowMs,
 *                  std::string &telOut, int64_t &userIdOut) override
 *     {
 *         std::string key = "session:" + token;
 *         redisReply *reply = (redisReply*)redisCommand(ctx_, "GET %s", key.c_str());
 *
 *         if (!reply || reply->type != REDIS_REPLY_STRING) {
 *             if (reply) freeReplyObject(reply);
 *             return false;
 *         }
 *
 *         std::string jsonStr(reply->str, reply->len);
 *         freeReplyObject(reply);
 *
 *         Json::CharReaderBuilder builder;
 *         Json::Value value;
 *         std::istringstream iss(jsonStr);
 *         std::string errs;
 *         if (!Json::parseFromStream(builder, iss, &value, &errs)) {
 *             return false;
 *         }
 *
 *         int64_t expireMs = value["expireMs"].asInt64();
 *         if (expireMs <= nowMs) {
 *             return false;
 *         }
 *
 *         telOut = value["tel"].asString();
 *         userIdOut = value["userId"].asInt64();
 *
 *         // 续期
 *         int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
 *         redisCommand(ctx_, "EXPIRE %s %d", key.c_str(), ttlSeconds);
 *
 *         return true;
 *     }
 * };
 */
