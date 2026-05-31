#include "../include/RedisSessionManager.hpp"
#include "../include/RedisConnPool.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include <jsoncpp/json/json.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>

#ifdef USE_HIREDIS
#include <hiredis/hiredis.h>
#endif

namespace hyperticket
{
    RedisSessionManager::RedisSessionManager(const std::string &redisHost,
                                             int redisPort,
                                             int64_t ttlMs,
                                             int poolSize)
        : redisHost_(redisHost), redisPort_(redisPort), ttlMs_(ttlMs)
    {
#ifdef USE_HIREDIS
        LOG_INFO << "Initializing RedisSessionManager with hiredis";
        LOG_INFO << "Redis config: " << redisHost << ":" << redisPort
                 << ", TTL=" << ttlMs / 1000 << "s, pool size=" << poolSize;

        try
        {
            pool_ = std::make_unique<RedisConnPool>(redisHost, redisPort, poolSize);
            LOG_INFO << "RedisSessionManager initialized successfully";
        }
        catch (const std::exception &e)
        {
            LOG_FATAL << "RedisSessionManager initialization failed: " << e.what();
            throw;
        }
#else
        LOG_WARN << "RedisSessionManager: hiredis not available, using placeholder implementation";
        LOG_WARN << "For production, install hiredis: sudo apt install libhiredis-dev";
        LOG_INFO << "Redis config: " << redisHost << ":" << redisPort << ", TTL=" << ttlMs / 1000 << "s";
        (void)poolSize;
#endif
    }

    RedisSessionManager::~RedisSessionManager()
    {
        LOG_INFO << "RedisSessionManager destroyed";
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

#ifdef USE_HIREDIS
        // 使用 Redis 存储
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_.get());

        std::string key = "session:" + token;
        int ttlSeconds = static_cast<int>(ttlMs_ / 1000);

        redisReply *reply = (redisReply *)redisCommand(ctx,
                                                       "SETEX %s %d %s",
                                                       key.c_str(),
                                                       ttlSeconds,
                                                       jsonStr.c_str());

        if (!reply || reply->type == REDIS_REPLY_ERROR)
        {
            LOG_ERROR << "Redis SETEX failed for token " << token.substr(0, 8) << "...";
            if (reply)
                freeReplyObject(reply);
            return "";
        }

        freeReplyObject(reply);
        LOG_DEBUG << "Session created in Redis: " << token.substr(0, 8) << "... for user " << userId;
#else
        // 占位实现：写入文件
        std::string filename = "/tmp/hyperticket_session_" + token;
        std::ofstream ofs(filename);
        if (ofs.is_open())
        {
            ofs << jsonStr;
            ofs.close();
        }
        LOG_DEBUG << "Session created (file): " << token.substr(0, 8) << "... for user " << userId;
#endif

        return token;
    }

    bool RedisSessionManager::resolve(const std::string &token, int64_t nowMs,
                                      std::string &telOut, int64_t &userIdOut)
    {
#ifdef USE_HIREDIS
        // 从 Redis 读取
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_.get());

        std::string key = "session:" + token;
        redisReply *reply = (redisReply *)redisCommand(ctx, "GET %s", key.c_str());

        if (!reply || reply->type != REDIS_REPLY_STRING)
        {
            if (reply)
                freeReplyObject(reply);
            return false;
        }

        std::string jsonStr(reply->str, reply->len);
        freeReplyObject(reply);

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
            return false;
        }

        telOut = value["tel"].asString();
        userIdOut = value["userId"].asInt64();

        // 续期：EXPIRE session:{token} {ttl_seconds}
        int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
        redisReply *expireReply = (redisReply *)redisCommand(ctx, "EXPIRE %s %d", key.c_str(), ttlSeconds);
        if (expireReply)
        {
            freeReplyObject(expireReply);
        }

        return true;
#else
        // 占位实现：从文件读取
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
#endif
    }

    void RedisSessionManager::purgeExpired(int64_t nowMs)
    {
        // Redis 自动过期，无需手动清理
        (void)nowMs;
    }

    void RedisSessionManager::remove(const std::string &token)
    {
#ifdef USE_HIREDIS
        // 从 Redis 删除
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_.get());

        std::string key = "session:" + token;
        redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());

        if (reply)
        {
            freeReplyObject(reply);
        }

        LOG_DEBUG << "Session removed from Redis: " << token.substr(0, 8) << "...";
#else
        // 占位实现：删除文件
        std::string filename = "/tmp/hyperticket_session_" + token;
        std::remove(filename.c_str());
        LOG_DEBUG << "Session removed (file): " << token.substr(0, 8) << "...";
#endif
    }

    bool RedisSessionManager::ping()
    {
#ifdef USE_HIREDIS
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            return pool_->ping(ctx);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis ping failed: " << e.what();
            return false;
        }
#else
        // 占位实现：总是返回 true
        return true;
#endif
    }

} // namespace hyperticket
