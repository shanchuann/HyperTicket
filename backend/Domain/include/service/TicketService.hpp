#ifndef HYPERTICKET_TICKET_SERVICE_HPP
#define HYPERTICKET_TICKET_SERVICE_HPP

#include <mutex>
#include <unordered_map>
#include <jsoncpp/json/json.h>

#include "../../../SqlConnPool/include/ConnectionPool.hpp"
#include "../ISessionManager.hpp"
#include "../repository/UserRepository.hpp"
#include "../repository/TicketRepository.hpp"
#include "../repository/ReservationRepository.hpp"
#include "../repository/AdminRepository.hpp"

namespace hyperticket
{
    class TicketService
    {
    public:
        TicketService(shanchuan::ConnectionPool *pool, ISessionManager *sessions)
            : pool_(pool), sessions_(sessions) {}

        Json::Value handle(const Json::Value &req);

        bool refreshTicketStatus();
        bool logStats();

    private:
        // 用户 handlers
        Json::Value login(const Json::Value &req);
        Json::Value reg(const Json::Value &req);
        Json::Value viewTickets();
        Json::Value orderTicket(const Json::Value &req);
        Json::Value viewMyTickets(const Json::Value &req);
        Json::Value cancelTicket(const Json::Value &req);

        // 管理员 handlers
        Json::Value adminLogin(const Json::Value &req);
        Json::Value adminListTickets(const Json::Value &req);
        Json::Value adminAddTicket(const Json::Value &req);
        Json::Value adminDeleteTicket(const Json::Value &req);
        Json::Value adminListUsers(const Json::Value &req);
        Json::Value adminStats(const Json::Value &req);
        Json::Value adminBlacklist(const Json::Value &req);

        // 管理员 token 管理
        std::string createAdminToken(const std::string &username);
        bool resolveAdminToken(const std::string &token, std::string &usernameOut);

        shanchuan::ConnectionPool *pool_;
        ISessionManager *sessions_;
        UserRepository userRepo_;
        TicketRepository ticketRepo_;
        ReservationRepository resvRepo_;
        AdminRepository adminRepo_;

        // 管理员会话（独立于用户会话，token → username）
        std::unordered_map<std::string, std::string> adminSessions_;
        std::mutex adminSessionsMtx_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_TICKET_SERVICE_HPP
