#ifndef HYPERTICKET_I_ORDER_QUEUE_HPP
#define HYPERTICKET_I_ORDER_QUEUE_HPP

#include <cstdint>
#include <string>

namespace hyperticket {

class IOrderQueue {
public:
    enum class EnqueueResult { Queued, SoldOut, Unavailable, Duplicate };
    struct Message {
        std::string id;
        std::string requestId;
        std::string payload;
    };
    struct Status {
        std::string state;
        std::string reservationId;
        std::string reason;
        int64_t userId = 0;
    };

    virtual ~IOrderQueue() = default;
    virtual EnqueueResult enqueue(int64_t ticketId, int quantity, int64_t userId,
                                  const std::string &requestId,
                                  const std::string &payload,
                                  std::string &streamId) = 0;
    virtual bool initializeStock(int64_t ticketId, int availableSeats) = 0;
    virtual bool consume(Message &message) = 0;
    virtual bool acknowledge(const std::string &streamId) = 0;
    virtual bool setStatus(const std::string &requestId, const std::string &state,
                           int64_t userId, const std::string &reservationId = "",
                           const std::string &reason = "") = 0;
    virtual bool getStatus(const std::string &requestId, Status &status) = 0;
    virtual bool requeue(const Message &message, const std::string &payload) = 0;
    virtual void recordFailure() = 0;
    virtual void recordCompensation() = 0;
    virtual void recordConsumeDuration(double seconds) = 0;
    virtual int deliveryCount(const std::string &streamId) = 0;
    virtual bool terminalFailure(const Message &message, int64_t ticketId,
                                 int quantity, int64_t userId,
                                 const std::string &reason) = 0;
};

} // namespace hyperticket
#endif
