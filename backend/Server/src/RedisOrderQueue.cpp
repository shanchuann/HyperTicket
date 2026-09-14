#include "../include/RedisOrderQueue.hpp"
#include "../include/RedisConnPool.hpp"

#include "../../ChronoLite/include/Logger.hpp"

#ifdef USE_HIREDIS
#include <hiredis/hiredis.h>
#endif

namespace hyperticket {

RedisOrderQueue::RedisOrderQueue(RedisConnPool *pool, std::string stream,
                                 std::string group, std::string consumer)
    : pool_(pool), stream_(std::move(stream)), group_(std::move(group)),
      consumer_(std::move(consumer)) {}

#ifdef USE_HIREDIS
namespace {
bool statusReplyOk(redisReply *reply, const char *accepted = nullptr) {
    if (!reply) return false;
    bool ok = reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING;
    if (ok && accepted && (!reply->str || std::string(reply->str) != accepted)) ok = false;
    return ok;
}
}
#endif

bool RedisOrderQueue::ensureGroup() {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_);
        redisReply *reply = static_cast<redisReply *>(redisCommand(
            ctx, "XGROUP CREATE %s %s 0 MKSTREAM", stream_.c_str(), group_.c_str()));
        if (!reply) return false;
        const bool ok = statusReplyOk(reply, "OK") ||
                        (reply->str && std::string(reply->str).find("BUSYGROUP") != std::string::npos);
        freeReplyObject(reply);
        return ok;
    } catch (const std::exception &e) {
        LOG_ERROR << "Redis order queue group init failed: " << e.what();
        return false;
    }
#else
    return false;
#endif
}

bool RedisOrderQueue::publish(const std::string &requestId, const std::string &payload,
                              std::string &streamId) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_);
        redisReply *reply = static_cast<redisReply *>(redisCommand(
            ctx, "XADD %s * request_id %b payload %b", stream_.c_str(),
            requestId.data(), requestId.size(), payload.data(), payload.size()));
        if (!reply || reply->type != REDIS_REPLY_STRING) {
            if (reply) freeReplyObject(reply);
            return false;
        }
        streamId.assign(reply->str, reply->len);
        freeReplyObject(reply);
        return !streamId.empty();
    } catch (const std::exception &e) {
        LOG_ERROR << "Redis order publish failed: " << e.what();
        return false;
    }
#else
    (void)requestId; (void)payload; (void)streamId;
    return false;
#endif
}

bool RedisOrderQueue::consume(Message &message, int blockMs) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_);
        redisReply *reply = static_cast<redisReply *>(redisCommand(
            ctx, "XREADGROUP GROUP %s %s COUNT 1 BLOCK %d STREAMS %s >",
            group_.c_str(), consumer_.c_str(), blockMs, stream_.c_str()));
        if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
            if (reply) freeReplyObject(reply);
            return false;
        }
        // [ [ stream, [ [id, [field, value, ...]] ] ] ]
        redisReply *stream = reply->element[0];
        if (!stream || stream->elements < 2 || !stream->element[1] ||
            stream->element[1]->elements == 0) { freeReplyObject(reply); return false; }
        redisReply *entry = stream->element[1]->element[0];
        if (!entry || entry->elements < 2) { freeReplyObject(reply); return false; }
        message.id = entry->element[0]->str ? entry->element[0]->str : "";
        redisReply *fields = entry->element[1];
        for (size_t i = 0; i + 1 < fields->elements; i += 2) {
            const std::string key = fields->element[i]->str ? fields->element[i]->str : "";
            const std::string value = fields->element[i + 1]->str ? fields->element[i + 1]->str : "";
            if (key == "request_id") message.requestId = value;
            else if (key == "payload") message.payload = value;
        }
        freeReplyObject(reply);
        return !message.id.empty() && !message.payload.empty();
    } catch (const std::exception &e) {
        LOG_ERROR << "Redis order consume failed: " << e.what();
        return false;
    }
#else
    (void)message; (void)blockMs;
    return false;
#endif
}

bool RedisOrderQueue::acknowledge(const std::string &streamId) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr; RedisConnGuard guard(&ctx, pool_);
        redisReply *reply = static_cast<redisReply *>(redisCommand(
            ctx, "XACK %s %s %s", stream_.c_str(), group_.c_str(), streamId.c_str()));
        const bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
        if (reply) freeReplyObject(reply);
        return ok;
    } catch (const std::exception &e) { LOG_ERROR << "Redis order ack failed: " << e.what(); return false; }
#else
    (void)streamId; return false;
#endif
}

bool RedisOrderQueue::retry(const Message &message) {
    std::string id;
    return publish(message.requestId, message.payload, id);
}

} // namespace hyperticket
