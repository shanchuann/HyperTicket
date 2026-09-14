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
        // 生成 32 字节随机 token（64 个 hex 字符）。
        // 优先 /dev/urandom（加密安全，与内存版 SessionManager 一致），
        // mt19937 可被预测，不能用于会话令牌。
        unsigned char buf[32];
        std::ifstream urandom("/dev/urandom", std::ios::binary);
        if (!urandom.good() ||
            !urandom.read(reinterpret_cast<char *>(buf), sizeof(buf)))
        {
            // 回退：random_device（多数平台仍为加密级实现）
            std::random_device rd;
            for (size_t i = 0; i < sizeof(buf); ++i)
            {
                buf[i] = static_cast<unsigned char>(rd() & 0xFF);
            }
        }
        static const char *hex = "0123456789abcdef";
        std::string out;
        out.reserve(sizeof(buf) * 2);
        for (size_t i = 0; i < sizeof(buf); ++i)
        {
            out.push_back(hex[buf[i] >> 4]);
            out.push_back(hex[buf[i] & 0x0F]);
        }
        return out;
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
        // 使用 Redis 存储。连接池可能抛异常（Redis 故障/取连接超时），
        // 捕获后返回空 token 表示失败，绝不让异常穿透 worker 线程。
        try
        {
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
            std::string userSetKey = "sessions:user:" + std::to_string(userId);
            reply = (redisReply *)redisCommand(ctx, "SADD %s %s", userSetKey.c_str(), token.c_str());
            if (!reply || reply->type == REDIS_REPLY_ERROR)
            {
                if (reply) freeReplyObject(reply);
                redisReply *cleanup = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());
                if (cleanup) freeReplyObject(cleanup);
                return "";
            }
            freeReplyObject(reply);
            reply = (redisReply *)redisCommand(ctx, "EXPIRE %s %d", userSetKey.c_str(), ttlSeconds);
            if (reply) freeReplyObject(reply);
            LOG_DEBUG << "Session created in Redis: " << token.substr(0, 8) << "... for user " << userId;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis session create failed: " << e.what();
            return "";
        }
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
        // 从 Redis 读取；池异常时视为解析失败（上层返回 UNAUTHORIZED）
        std::string jsonStr;
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());

            std::string key = "session:" + token;
            // GETEX 原子地 GET + 续期，避免 GET 后 key 在 EXPIRE 前过期的竞态
            int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
            redisReply *reply = (redisReply *)redisCommand(
                ctx, "GETEX %s EX %d", key.c_str(), ttlSeconds);

            if (!reply || reply->type != REDIS_REPLY_STRING)
            {
                if (reply)
                    freeReplyObject(reply);
                return false;
            }

            jsonStr.assign(reply->str, reply->len);
            freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis session resolve failed: " << e.what();
            return false;
        }

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

        // Redis TTL 已经处理过期，这里不需要再检查 expireMs
        // 如果能读取到，说明还没过期
        telOut = value["tel"].asString();
        userIdOut = value["userId"].asInt64();
        // 续期反向索引，供修改密码后撤销该用户的所有会话。
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            std::string userSetKey = "sessions:user:" + std::to_string(userIdOut);
            int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
            redisReply *reply = (redisReply *)redisCommand(ctx, "EXPIRE %s %d", userSetKey.c_str(), ttlSeconds);
            if (reply) freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_WARN << "Redis session index refresh failed: " << e.what();
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
        // 从 Redis 删除；失败仅记录日志（登出幂等，token 到 TTL 也会自动过期）
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());

            std::string key = "session:" + token;
            int64_t userId = 0;
            redisReply *current = (redisReply *)redisCommand(ctx, "GET %s", key.c_str());
            if (current && current->type == REDIS_REPLY_STRING)
            {
                Json::CharReaderBuilder builder;
                Json::Value value;
                std::istringstream iss(std::string(current->str, current->len));
                std::string errors;
                if (Json::parseFromStream(builder, iss, &value, &errors))
                    userId = value.get("userId", 0).asInt64();
            }
            if (current) freeReplyObject(current);

            redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());

            if (reply)
            {
                freeReplyObject(reply);
            }
            if (userId > 0)
            {
                std::string userSetKey = "sessions:user:" + std::to_string(userId);
                reply = (redisReply *)redisCommand(ctx, "SREM %s %s", userSetKey.c_str(), token.c_str());
                if (reply) freeReplyObject(reply);
            }

            LOG_DEBUG << "Session removed from Redis: " << token.substr(0, 8) << "...";
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis session remove failed: " << e.what();
        }
#else
        // 占位实现：删除文件
        std::string filename = "/tmp/hyperticket_session_" + token;
        std::remove(filename.c_str());
        LOG_DEBUG << "Session removed (file): " << token.substr(0, 8) << "...";
#endif
    }

    void RedisSessionManager::removeAllForUser(int64_t userId)
    {
#ifdef USE_HIREDIS
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            std::string setKey = "sessions:user:" + std::to_string(userId);
            redisReply *members = (redisReply *)redisCommand(ctx, "SMEMBERS %s", setKey.c_str());
            if (members && members->type == REDIS_REPLY_ARRAY)
            {
                for (size_t i = 0; i < members->elements; ++i)
                {
                    std::string tokenValue(members->element[i]->str, members->element[i]->len);
                    std::string key = "session:" + tokenValue;
                    redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());
                    if (reply) freeReplyObject(reply);
                }
            }
            if (members) freeReplyObject(members);
            redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", setKey.c_str());
            if (reply) freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis remove all user sessions failed: " << e.what();
        }
#else
        (void)userId;
#endif
    }

    std::string RedisSessionManager::createAdmin(const std::string &username,
                                                  bool mustChangePassword,
                                                  int64_t nowMs)
    {
        std::string token = "adm_" + generateToken();
        Json::Value value;
        value["username"] = username;
        value["mustChangePassword"] = mustChangePassword;
        value["expireMs"] = static_cast<Json::Int64>(nowMs + ttlMs_);
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string payload = Json::writeString(builder, value);
#ifdef USE_HIREDIS
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            std::string key = "admin_session:" + token;
            std::string setKey = "admin_sessions:" + username;
            int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
            redisReply *reply = (redisReply *)redisCommand(ctx, "SETEX %s %d %s", key.c_str(), ttlSeconds, payload.c_str());
            if (!reply || reply->type == REDIS_REPLY_ERROR)
            {
                if (reply) freeReplyObject(reply);
                return "";
            }
            freeReplyObject(reply);
            reply = (redisReply *)redisCommand(ctx, "SADD %s %s", setKey.c_str(), token.c_str());
            if (reply) freeReplyObject(reply);
            reply = (redisReply *)redisCommand(ctx, "EXPIRE %s %d", setKey.c_str(), ttlSeconds);
            if (reply) freeReplyObject(reply);
            return token;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis admin session create failed: " << e.what();
            return "";
        }
#else
        std::ofstream ofs("/tmp/hyperticket_admin_session_" + token);
        if (!ofs.is_open()) return "";
        ofs << payload;
        return token;
#endif
    }

    bool RedisSessionManager::resolveAdmin(const std::string &token, int64_t nowMs,
                                            std::string &usernameOut,
                                            bool &mustChangePasswordOut)
    {
        std::string payload;
#ifdef USE_HIREDIS
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            std::string key = "admin_session:" + token;
            int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
            redisReply *reply = (redisReply *)redisCommand(ctx, "GETEX %s EX %d", key.c_str(), ttlSeconds);
            if (!reply || reply->type != REDIS_REPLY_STRING)
            {
                if (reply) freeReplyObject(reply);
                return false;
            }
            payload.assign(reply->str, reply->len);
            freeReplyObject(reply);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis admin session resolve failed: " << e.what();
            return false;
        }
#else
        std::ifstream ifs("/tmp/hyperticket_admin_session_" + token);
        if (!ifs.is_open()) return false;
        std::ostringstream oss;
        oss << ifs.rdbuf();
        payload = oss.str();
#endif
        Json::CharReaderBuilder builder;
        Json::Value value;
        std::istringstream iss(payload);
        std::string errors;
        if (!Json::parseFromStream(builder, iss, &value, &errors)) return false;
#ifndef USE_HIREDIS
        if (value.get("expireMs", 0).asInt64() <= nowMs)
        {
            removeAdmin(token);
            return false;
        }
#endif
        usernameOut = value.get("username", "").asString();
        mustChangePasswordOut = value.get("mustChangePassword", false).asBool();
#ifdef USE_HIREDIS
        if (!usernameOut.empty())
        {
            try
            {
                redisContext *ctx = nullptr;
                RedisConnGuard guard(&ctx, pool_.get());
                std::string setKey = "admin_sessions:" + usernameOut;
                int ttlSeconds = static_cast<int>(ttlMs_ / 1000);
                redisReply *reply = (redisReply *)redisCommand(ctx, "EXPIRE %s %d", setKey.c_str(), ttlSeconds);
                if (reply) freeReplyObject(reply);
            }
            catch (const std::exception &e)
            {
                LOG_WARN << "Redis admin session index refresh failed: " << e.what();
            }
        }
#endif
        return !usernameOut.empty();
    }

    void RedisSessionManager::removeAdmin(const std::string &token)
    {
#ifdef USE_HIREDIS
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            std::string key = "admin_session:" + token;
            redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());
            if (reply) freeReplyObject(reply);
        }
        catch (const std::exception &e) { LOG_ERROR << "Redis admin session remove failed: " << e.what(); }
#else
        std::remove(("/tmp/hyperticket_admin_session_" + token).c_str());
#endif
    }

    void RedisSessionManager::removeAllForAdmin(const std::string &username)
    {
#ifdef USE_HIREDIS
        try
        {
            redisContext *ctx = nullptr;
            RedisConnGuard guard(&ctx, pool_.get());
            std::string setKey = "admin_sessions:" + username;
            redisReply *members = (redisReply *)redisCommand(ctx, "SMEMBERS %s", setKey.c_str());
            if (members && members->type == REDIS_REPLY_ARRAY)
            {
                for (size_t i = 0; i < members->elements; ++i)
                {
                    std::string tokenValue(members->element[i]->str, members->element[i]->len);
                    std::string key = "admin_session:" + tokenValue;
                    redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());
                    if (reply) freeReplyObject(reply);
                }
            }
            if (members) freeReplyObject(members);
            redisReply *reply = (redisReply *)redisCommand(ctx, "DEL %s", setKey.c_str());
            if (reply) freeReplyObject(reply);
        }
        catch (const std::exception &e) { LOG_ERROR << "Redis remove all admin sessions failed: " << e.what(); }
#else
        (void)username;
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
