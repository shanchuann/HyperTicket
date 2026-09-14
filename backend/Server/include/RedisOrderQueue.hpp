#ifndef HYPERTICKET_REDIS_ORDER_QUEUE_HPP
#define HYPERTICKET_REDIS_ORDER_QUEUE_HPP

#include <string>

namespace hyperticket {

class RedisConnPool;

// Redis Streams order queue. The queue only transports an immutable order
// request; database side effects are performed by the consumer.
class RedisOrderQueue {
public:
    struct Message {
        std::string id;
        std::string requestId;
        std::string payload;
    };

    RedisOrderQueue(RedisConnPool *pool, std::string stream,
                    std::string group, std::string consumer);

    // Creates the consumer group if it does not exist. BUSYGROUP is treated
    // as success so this operation is safe on every process start.
    bool ensureGroup();

    // Append a request to the stream. Returns the Redis stream id.
    bool publish(const std::string &requestId, const std::string &payload,
                 std::string &streamId);

    // Read one message for this consumer. blockMs=0 performs a non-blocking
    // read; positive values use XREADGROUP BLOCK.
    bool consume(Message &message, int blockMs = 1000);

    bool acknowledge(const std::string &streamId);
    bool retry(const Message &message);

private:
    RedisConnPool *pool_;
    std::string stream_;
    std::string group_;
    std::string consumer_;
};

} // namespace hyperticket

#endif
