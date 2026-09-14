#include "../include/RedisOrderQueue.hpp"
#include "../include/RedisConnPool.hpp"
#include "../include/MetricsManager.hpp"

#include "../../ChronoLite/include/Logger.hpp"

#ifdef USE_HIREDIS
#include <hiredis/hiredis.h>
#endif
#include <cstdlib>
#include <utility>

namespace hyperticket {

RedisOrderQueue::RedisOrderQueue(RedisConnPool *pool, std::string stream,
                                 std::string group, std::string consumer,
                                 MetricsManager *metrics, int claimIdleMs)
    : pool_(pool), stream_(std::move(stream)), group_(std::move(group)),
      consumer_(std::move(consumer)), metrics_(metrics), claimIdleMs_(claimIdleMs) {}

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

IOrderQueue::EnqueueResult RedisOrderQueue::enqueue(
    int64_t ticketId, int quantity, int64_t userId, const std::string &requestId,
    const std::string &payload, std::string &streamId) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_);
        const std::string stockKey = "stock:" + std::to_string(ticketId);
        const std::string statusKey = "order:req:" + requestId;
        static const char *script =
            "if redis.call('EXISTS',KEYS[3])==1 then return 'DUPLICATE' end "
            "local v=redis.call('GET',KEYS[1]) "
            "if not v then return 'UNAVAILABLE' end "
            "local q=tonumber(ARGV[1]) "
            "if tonumber(v)<q then return 'SOLD_OUT' end "
            "redis.call('DECRBY',KEYS[1],q) "
            "local id=redis.call('XADD',KEYS[2],'MAXLEN','~','100000','*','request_id',ARGV[2],'payload',ARGV[3]) "
            "redis.call('HSET',KEYS[3],'state','QUEUED','user_id',ARGV[4],'stream_id',id) "
            "redis.call('EXPIRE',KEYS[3],ARGV[5]) return id";
        redisReply *reply = static_cast<redisReply *>(redisCommand(
            ctx, "EVAL %s 3 %s %s %s %d %b %b %s %d", script,
            stockKey.c_str(), stream_.c_str(), statusKey.c_str(), quantity,
            requestId.data(), requestId.size(), payload.data(), payload.size(),
            std::to_string(userId).c_str(), kStatusTtlSeconds));
        if (!reply || reply->type != REDIS_REPLY_STRING) {
            if (reply) freeReplyObject(reply);
            return EnqueueResult::Unavailable;
        }
        streamId.assign(reply->str, reply->len);
        freeReplyObject(reply);
        if (streamId == "SOLD_OUT") { if(metrics_)metrics_->recordInventorySoldOut(); return EnqueueResult::SoldOut; }
        if (streamId == "UNAVAILABLE") return EnqueueResult::Unavailable;
        if (streamId == "DUPLICATE") return EnqueueResult::Duplicate;
        if(!streamId.empty() && metrics_)metrics_->recordOrderQueued();
        return streamId.empty() ? EnqueueResult::Unavailable : EnqueueResult::Queued;
    } catch (const std::exception &e) {
        LOG_ERROR << "Redis order publish failed: " << e.what();
        return EnqueueResult::Unavailable;
    }
#else
    (void)ticketId; (void)quantity; (void)userId; (void)requestId; (void)payload; (void)streamId;
    return EnqueueResult::Unavailable;
#endif
}

bool RedisOrderQueue::initializeStock(int64_t ticketId, int availableSeats) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx=nullptr; RedisConnGuard guard(&ctx,pool_);
        const std::string key="stock:"+std::to_string(ticketId);
        redisReply *reply=static_cast<redisReply *>(redisCommand(
            ctx,"SET %s %d NX",key.c_str(),availableSeats));
        const bool ok=reply && (reply->type==REDIS_REPLY_STATUS || reply->type==REDIS_REPLY_NIL);
        if(reply)freeReplyObject(reply); return ok;
    } catch (...) { return false; }
#else
    (void)ticketId;(void)availableSeats; return false;
#endif
}

bool RedisOrderQueue::consume(Message &message) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr;
        RedisConnGuard guard(&ctx, pool_);
        auto parseEntry = [&](redisReply *entry) {
            if (!entry || entry->elements < 2) return false;
            message.id = entry->element[0]->str ? entry->element[0]->str : "";
            redisReply *fields = entry->element[1];
            for (size_t i=0; fields && i+1<fields->elements; i+=2) {
                const std::string key=fields->element[i]->str ? fields->element[i]->str : "";
                const std::string value=fields->element[i+1]->str ? fields->element[i+1]->str : "";
                if(key=="request_id")message.requestId=value;
                else if(key=="payload")message.payload=value;
            }
            return !message.id.empty() && !message.payload.empty();
        };
        auto parseRead = [&](redisReply *r) {
            if(!r || r->type!=REDIS_REPLY_ARRAY || r->elements==0)return false;
            redisReply *stream=r->element[0];
            return stream && stream->elements>=2 && stream->element[1] &&
                   stream->element[1]->elements>0 && parseEntry(stream->element[1]->element[0]);
        };
        // Recover abandoned entries from crashed consumers after the idle
        // threshold; this also works when a restarted process uses the same name.
        redisReply *reply=static_cast<redisReply *>(redisCommand(ctx,
            "XAUTOCLAIM %s %s %s %d 0-0 COUNT 1",stream_.c_str(),group_.c_str(),
            consumer_.c_str(),claimIdleMs_));
        bool found=false;
        if(reply && reply->type==REDIS_REPLY_ARRAY && reply->elements>=2 &&
           reply->element[1] && reply->element[1]->elements>0)
            found=parseEntry(reply->element[1]->element[0]);
        if(reply)freeReplyObject(reply); if(found)return true;

        reply=static_cast<redisReply *>(redisCommand(ctx,
            "XREADGROUP GROUP %s %s COUNT 1 STREAMS %s >",
            group_.c_str(),consumer_.c_str(),stream_.c_str()));
        found=parseRead(reply); if(reply)freeReplyObject(reply); return found;
    } catch (const std::exception &e) {
        LOG_ERROR << "Redis order consume failed: " << e.what();
        return false;
    }
#else
    (void)message;
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
        if(ok && metrics_)metrics_->recordOrderConsumed();
        return ok;
    } catch (const std::exception &e) { LOG_ERROR << "Redis order ack failed: " << e.what(); return false; }
#else
    (void)streamId; return false;
#endif
}

void RedisOrderQueue::recordFailure() { if(metrics_)metrics_->recordOrderQueueFailure(); }
void RedisOrderQueue::recordCompensation() { if(metrics_)metrics_->recordInventoryCompensation(); }
void RedisOrderQueue::recordConsumeDuration(double seconds) { if(metrics_)metrics_->recordOrderQueueDuration(seconds); }

int RedisOrderQueue::deliveryCount(const std::string &streamId) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx=nullptr; RedisConnGuard guard(&ctx,pool_);
        redisReply *reply=static_cast<redisReply *>(redisCommand(ctx,
            "XPENDING %s %s %s %s 1",stream_.c_str(),group_.c_str(),streamId.c_str(),streamId.c_str()));
        int count=0;
        if(reply && reply->type==REDIS_REPLY_ARRAY && reply->elements==1 &&
           reply->element[0]->elements>=4) count=static_cast<int>(reply->element[0]->element[3]->integer);
        if(reply)freeReplyObject(reply); return count;
    } catch (...) { return 0; }
#else
    (void)streamId; return 0;
#endif
}

bool RedisOrderQueue::terminalFailure(const Message &message, int64_t ticketId,
                                      int quantity, int64_t userId,
                                      const std::string &reason) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx=nullptr; RedisConnGuard guard(&ctx,pool_);
        const std::string stock="stock:"+std::to_string(ticketId);
        const std::string status="order:req:"+message.requestId;
        const std::string dead=stream_+":dead";
        static const char *script=
          "if tonumber(ARGV[1])>0 and redis.call('EXISTS',KEYS[1])==1 then redis.call('INCRBY',KEYS[1],ARGV[1]) end "
          "redis.call('XADD',KEYS[2],'MAXLEN','~','10000','*','request_id',ARGV[2],'payload',ARGV[3],'reason',ARGV[4]) "
          "redis.call('HSET',KEYS[3],'state','FAILED','user_id',ARGV[5],'reason',ARGV[4]) "
          "redis.call('EXPIRE',KEYS[3],ARGV[6]) return redis.call('XACK',KEYS[4],ARGV[7],ARGV[8])";
        redisReply *reply=static_cast<redisReply *>(redisCommand(ctx,
          "EVAL %s 4 %s %s %s %s %d %b %b %b %lld %d %s %s",script,
          stock.c_str(),dead.c_str(),status.c_str(),stream_.c_str(),quantity,
          message.requestId.data(),message.requestId.size(),message.payload.data(),message.payload.size(),
          reason.data(),reason.size(),static_cast<long long>(userId),kStatusTtlSeconds,
          group_.c_str(),message.id.c_str()));
        const bool ok=reply && reply->type==REDIS_REPLY_INTEGER && reply->integer==1;
        if(reply)freeReplyObject(reply);
        if(ok){recordFailure();recordCompensation();if(metrics_)metrics_->recordOrderConsumed();}
        return ok;
    } catch (...) { return false; }
#else
    (void)message;(void)ticketId;(void)quantity;(void)userId;(void)reason;return false;
#endif
}

bool RedisOrderQueue::setStatus(const std::string &requestId, const std::string &state,
                                int64_t userId, const std::string &reservationId,
                                const std::string &reason) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr; RedisConnGuard guard(&ctx, pool_);
        const std::string key = "order:req:" + requestId;
        redisReply *reply = static_cast<redisReply *>(redisCommand(ctx,
            "MULTI")); if (reply) freeReplyObject(reply);
        reply = static_cast<redisReply *>(redisCommand(ctx,
            "HSET %s state %b user_id %lld reservation_id %b reason %b", key.c_str(),
            state.data(), state.size(), static_cast<long long>(userId),
            reservationId.data(), reservationId.size(), reason.data(), reason.size()));
        if (reply) freeReplyObject(reply);
        reply = static_cast<redisReply *>(redisCommand(ctx, "EXPIRE %s %d", key.c_str(), kStatusTtlSeconds));
        if (reply) freeReplyObject(reply);
        reply = static_cast<redisReply *>(redisCommand(ctx, "EXEC"));
        const bool ok = reply && reply->type == REDIS_REPLY_ARRAY;
        if (reply) freeReplyObject(reply); return ok;
    } catch (...) { return false; }
#else
    (void)requestId;(void)state;(void)userId;(void)reservationId;(void)reason; return false;
#endif
}

bool RedisOrderQueue::getStatus(const std::string &requestId, Status &status) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx = nullptr; RedisConnGuard guard(&ctx, pool_);
        const std::string key = "order:req:" + requestId;
        redisReply *reply = static_cast<redisReply *>(redisCommand(ctx,
            "HMGET %s state user_id reservation_id reason", key.c_str()));
        if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 4 ||
            reply->element[0]->type == REDIS_REPLY_NIL) { if(reply) freeReplyObject(reply); return false; }
        auto str = [&](size_t i) { auto *e=reply->element[i]; return e->str ? std::string(e->str,e->len) : std::string(); };
        status.state=str(0); status.userId=std::strtoll(str(1).c_str(),nullptr,10);
        status.reservationId=str(2); status.reason=str(3); freeReplyObject(reply); return true;
    } catch (...) { return false; }
#else
    (void)requestId;(void)status; return false;
#endif
}

bool RedisOrderQueue::requeue(const Message &message, const std::string &payload) {
#ifdef USE_HIREDIS
    try {
        redisContext *ctx=nullptr; RedisConnGuard guard(&ctx,pool_);
        redisReply *reply=static_cast<redisReply *>(redisCommand(ctx,
            "XADD %s MAXLEN ~ 100000 * request_id %b payload %b",stream_.c_str(),message.requestId.data(),message.requestId.size(),payload.data(),payload.size()));
        const bool ok=reply && reply->type==REDIS_REPLY_STRING; if(reply)freeReplyObject(reply); return ok;
    } catch (...) { return false; }
#else
    (void)message;(void)payload; return false;
#endif
}

} // namespace hyperticket
