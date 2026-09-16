#ifndef HYPERTICKET_ENTITIES_HPP
#define HYPERTICKET_ENTITIES_HPP

#include <cstdint>
#include <string>

namespace hyperticket
{
    struct User
    {
        int64_t id = 0;
        std::string tel;
        std::string username;
        std::string passwordHash;
        std::string email;
        bool emailVerified = false;
        bool phoneVerified = false;
        int status = 1;
    };

    struct Ticket
    {
        int64_t id = 0;
        std::string title;
        std::string venue;
        int totalSeats = 0;
        int availableSeats = 0;
        std::string eventDate;
        int status = 1;
        std::string coverImage;
        std::string category; // concert/sports/esports/movie/theater/exhibition
        int price = 0;        // base price (yuan)
        std::string city;
        std::string description; // 详情简介
        std::string notice;      // 购票须知
        std::string artist;      // 艺人/团体
        int hotScore = 0;        // 热门榜：有效订单数（仅 HOT_TICKETS 填充）
    };

    struct Reservation
    {
        int64_t id = 0;
        int64_t userId = 0;
        int64_t ticketId = 0;
        int quantity = 1;
        std::string status;
        std::string orderNo;     // 真实订单号 HT{YYYYMMDD}{ID:06d}
        std::string ticketTitle;
        std::string ticketVenue;
        std::string eventDate;
        std::string createdAt;
        std::string category;
        std::string seatLabel;
        std::string seatTier;
        int seatPrice = 0;
        std::string expireAt;    // PENDING 订单支付截止时间
        int ticketPrice = 0;     // 票面单价（计算总价用）
    };

    struct Payment
    {
        int64_t id = 0;
        std::string paymentNo;   // 支付单号 PY{YYYYMMDD}{ID:08d}
        int64_t reservationId = 0;
        int64_t userId = 0;
        int64_t amountMinor = 0; // 最小货币单位，CNY 时为分
        std::string currency = "CNY";
        std::string provider;    // MOCK / ALIPAY / WECHAT
        std::string providerTransactionId;
        std::string idempotencyKey;
        std::string status;      // CREATED/PROCESSING/SUCCEEDED/FAILED/CLOSED/REFUNDING/PARTIALLY_REFUNDED/REFUNDED
        std::string createdAt;
    };

    struct Refund
    {
        int64_t id = 0;
        int64_t paymentId = 0;
        std::string refundNo;
        int64_t amountMinor = 0;
        std::string currency = "CNY";
        std::string status;
        std::string paymentNo;
        std::string provider;
        std::string paymentProviderTransactionId;
        std::string providerRefundId;
        int attemptCount = 0;
        int maxAttempts = 5;
    };

    struct Seat
    {
        int64_t id = 0;
        int64_t ticketId = 0;
        std::string seatLabel;
        std::string rowLabel;
        int colNum = 0;
        std::string tier; // VIP / Standard / Economy
        int price = 0;
        std::string status; // AVAILABLE / SOLD
        int64_t reservationId = 0;
    };

    struct Admin
    {
        int64_t id = 0;
        std::string username;
        std::string passwordHash;
        std::string role;
    };

    struct AuthChallenge
    {
        std::string id;
        int64_t userId = 0;
        std::string subject;
        std::string destination;
        std::string purpose;
        std::string channel;
        std::string codeHash;
        std::string grantHash;
        int attempts = 0;
        int maxAttempts = 0;
    };
} // namespace hyperticket
#endif // HYPERTICKET_ENTITIES_HPP
