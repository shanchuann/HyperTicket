#ifndef HYPERTICKET_REDIS_ORDER_QUEUE_HPP
#define HYPERTICKET_REDIS_ORDER_QUEUE_HPP

#include <string>
#include "../../Domain/include/IOrderQueue.hpp"

namespace hyperticket {

class RedisConnPool;
class MetricsManager;

// Redis Streams order queue. The queue only transports an immutable order
// request; database side effects are performed by the consumer.
class RedisOrderQueue : public IOrderQueue {
public:
    RedisOrderQueue(RedisConnPool *pool, std::string stream,
                    std::string group, std::string consumer,
                    MetricsManager *metrics = nullptr, int claimIdleMs = 30000);

    // Creates the consumer group if it does not exist. BUSYGROUP is treated
    // as success so this operation is safe on every process start.
    bool ensureGroup();

    // Append a request to the stream. Returns the Redis stream id.
    EnqueueResult enqueue(int64_t ticketId, int quantity, int64_t userId,
                          const std::string &requestId,
                          const std::string &payload,
                          std::string &streamId) override;
    bool initializeStock(int64_t ticketId, int availableSeats) override;

    // Read one message for this consumer. blockMs=0 performs a non-blocking
    // read; positive values use XREADGROUP BLOCK.
    bool consume(Message &message) override;

    bool acknowledge(const std::string &streamId) override;
    bool setStatus(const std::string &requestId, const std::string &state,
                   int64_t userId, const std::string &reservationId = "",
                   const std::string &reason = "") override;
    bool getStatus(const std::string &requestId, Status &status) override;
    bool requeue(const Message &message, const std::string &payload) override;
    void recordFailure() override;
    void recordCompensation() override;
    void recordConsumeDuration(double seconds) override;
    int deliveryCount(const std::string &streamId) override;
    bool terminalFailure(const Message &message, int64_t ticketId, int quantity,
                         int64_t userId, const std::string &reason) override;

private:
    RedisConnPool *pool_;
    std::string stream_;
    std::string group_;
    std::string consumer_;
    MetricsManager *metrics_;
    int claimIdleMs_;
    static constexpr int kStatusTtlSeconds = 86400;
};

} // namespace hyperticket

#endif
