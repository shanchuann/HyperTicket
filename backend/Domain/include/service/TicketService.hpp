#ifndef HYPERTICKET_TICKET_SERVICE_HPP
#define HYPERTICKET_TICKET_SERVICE_HPP

#include <jsoncpp/json/json.h>

#include "../../../SqlConnPool/include/ConnectionPool.hpp"
#include "../ISessionManager.hpp"
#include "../IStockCache.hpp"
#include "../IOrderQueue.hpp"
#include "../IVerificationSender.hpp"
#include "../repository/UserRepository.hpp"
#include "../repository/TicketRepository.hpp"
#include "../repository/ReservationRepository.hpp"
#include "../repository/PaymentRepository.hpp"
#include "../repository/AdminRepository.hpp"
#include "../repository/SeatRepository.hpp"
#include "../repository/FavoriteRepository.hpp"
#include "../repository/AuthSecurityRepository.hpp"
#include "../repository/AuthChallengeRepository.hpp"

namespace hyperticket
{
    class TicketService
    {
    public:
        // stock 可为空：为空时使用内置 NoopStockCache（无缓存直连 DB）。
        TicketService(shanchuan::ConnectionPool *pool, ISessionManager *sessions,
                      IStockCache *stock = nullptr, IVerificationSender *verificationSender = nullptr)
            : pool_(pool), sessions_(sessions),
              stock_(stock ? stock : &noopStock_), verificationSender_(verificationSender) {}

        Json::Value handle(const Json::Value &req);

        bool refreshTicketStatus();
        bool logStats();
        // 定时回收超时未支付的 PENDING 订单（回补 DB 与 Redis 库存）。
        bool expirePendingOrders();
        // 定时结算到期的 PROCESSING 支付流水（模拟网关异步回调）。
        bool settleDuePayments();
        void configureOrderQueue(IOrderQueue *queue, int maxRetries)
        { orderQueue_ = queue; orderQueueMaxRetries_ = maxRetries; }
        int processQueuedOrders(int maxMessages);
        bool purgeAuthenticationState();
        // 模拟网关参数（config.json payment 段）：结算延迟与成功率。
        void configurePayment(int settleDelayMs, int successRatePercent)
        {
            paySettleDelayMs_ = settleDelayMs;
            paySuccessRatePercent_ = successRatePercent;
        }
        void configureAuth(int maxFailures, int failureWindowSeconds, int lockSeconds)
        {
            authMaxFailures_ = maxFailures > 0 ? maxFailures : 5;
            authFailureWindowSeconds_ = failureWindowSeconds > 0 ? failureWindowSeconds : 900;
            authLockSeconds_ = lockSeconds > 0 ? lockSeconds : 900;
        }
        void configureVerification(bool mockSmsEnabled, bool exposeMockSmsCode,
                                   int codeTtlSeconds, int maxAttempts,
                                   int resendCooldownSeconds, int grantTtlSeconds,
                                   bool requireRegistrationVerification)
        {
            mockSmsEnabled_ = mockSmsEnabled;
            exposeMockSmsCode_ = exposeMockSmsCode;
            codeTtlSeconds_ = codeTtlSeconds;
            verificationMaxAttempts_ = maxAttempts;
            resendCooldownSeconds_ = resendCooldownSeconds;
            grantTtlSeconds_ = grantTtlSeconds;
            requireRegistrationVerification_ = requireRegistrationVerification;
        }

    private:
        // 用户 handlers
        Json::Value login(const Json::Value &req);
        Json::Value reg(const Json::Value &req);
        Json::Value logout(const Json::Value &req);
        Json::Value viewTickets(const Json::Value &req);
        Json::Value ticketDetail(const Json::Value &req);
        Json::Value orderTicket(const Json::Value &req);
        Json::Value payOrder(const Json::Value &req);
        Json::Value queryPayment(const Json::Value &req);
        Json::Value viewMyTickets(const Json::Value &req);
        Json::Value cancelTicket(const Json::Value &req);
        Json::Value deleteOrder(const Json::Value &req);
        Json::Value viewSeats(const Json::Value &req);
        Json::Value verifyOrder(const Json::Value &req);
        Json::Value favorite(const Json::Value &req);
        Json::Value viewFavorites(const Json::Value &req);
        Json::Value hotTickets(const Json::Value &req);
        Json::Value queryQueuedOrder(const Json::Value &req);
        Json::Value requestVerification(const Json::Value &req);
        Json::Value verifyRegistrationCode(const Json::Value &req);
        Json::Value requestPasswordReset(const Json::Value &req);
        Json::Value verifyPasswordReset(const Json::Value &req);
        Json::Value confirmPasswordReset(const Json::Value &req);
        Json::Value accountSecurityStatus(const Json::Value &req);
        Json::Value requestContactVerification(const Json::Value &req);
        Json::Value confirmContactVerification(const Json::Value &req);
        bool consumeGrant(MYSQL *conn, const std::string &token,
                          const std::string &purpose, AuthChallenge &challengeOut);

        // 管理员 handlers
        Json::Value adminLogin(const Json::Value &req);
        Json::Value adminListTickets(const Json::Value &req);
        Json::Value adminAddTicket(const Json::Value &req);
        Json::Value adminDeleteTicket(const Json::Value &req);
        Json::Value adminListUsers(const Json::Value &req);
        Json::Value adminStats(const Json::Value &req);
        Json::Value adminBlacklist(const Json::Value &req);
        Json::Value adminChangePassword(const Json::Value &req);

        // 管理员 token 管理
        std::string createAdminToken(const std::string &username, bool mustChangePassword);
        bool resolveAdminToken(const std::string &token, std::string &usernameOut,
                               bool &mustChangePasswordOut);
        bool authorizeAdmin(const Json::Value &req, std::string &usernameOut,
                            std::string &errorOut,
                            bool allowPasswordChangeOnly = false);

        shanchuan::ConnectionPool *pool_;
        ISessionManager *sessions_;
        NoopStockCache noopStock_; // stock 未注入时的兜底实现
        IStockCache *stock_;
        IOrderQueue *orderQueue_ = nullptr;
        int orderQueueMaxRetries_ = 5;
        UserRepository userRepo_;
        TicketRepository ticketRepo_;
        ReservationRepository resvRepo_;
        PaymentRepository payRepo_;
        AdminRepository adminRepo_;
        SeatRepository seatRepo_;
        FavoriteRepository favRepo_;
        AuthSecurityRepository authRepo_;
        AuthChallengeRepository challengeRepo_;
        IVerificationSender *verificationSender_ = nullptr;

        // 模拟支付网关参数（可由 configurePayment 覆盖）
        int paySettleDelayMs_ = 1000;      // 发起支付 → 网关结算的延迟
        int paySuccessRatePercent_ = 100;  // 结算成功率（0-100），<100 用于演练失败路径

        int authMaxFailures_ = 5;
        int authFailureWindowSeconds_ = 900;
        int authLockSeconds_ = 900;
        bool mockSmsEnabled_ = false;
        bool exposeMockSmsCode_ = false;
        int codeTtlSeconds_ = 300;
        int verificationMaxAttempts_ = 5;
        int resendCooldownSeconds_ = 60;
        int grantTtlSeconds_ = 600;
        bool requireRegistrationVerification_ = true;
    };
} // namespace hyperticket
#endif // HYPERTICKET_TICKET_SERVICE_HPP
