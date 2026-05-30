#ifndef HYPERTICKET_TICKET_SERVICE_HPP
#define HYPERTICKET_TICKET_SERVICE_HPP

#include <jsoncpp/json/json.h>

#include "../../../SqlConnPool/include/ConnectionPool.hpp"
#include "../../../Server/include/SessionManager.hpp"
#include "../repository/UserRepository.hpp"
#include "../repository/TicketRepository.hpp"
#include "../repository/ReservationRepository.hpp"

namespace hyperticket
{
    // 服务端业务编排层：鉴权（SessionManager）+ 事务边界（BEGIN/COMMIT/ROLLBACK）
    // + 调用 Repository + 用 Protocol 构造响应。不含裸 SQL，不含协议魔法字符串。
    class TicketService
    {
    public:
        TicketService(shanchuan::ConnectionPool *pool, SessionManager *sessions)
            : pool_(pool), sessions_(sessions) {}

        // 请求总入口：按 type 分发。
        Json::Value handle(const Json::Value &req);

        // 定时任务复用的维护操作。
        bool refreshTicketStatus();
        bool logStats();

    private:
        Json::Value login(const Json::Value &req);
        Json::Value reg(const Json::Value &req);
        Json::Value viewTickets();
        Json::Value orderTicket(const Json::Value &req);
        Json::Value viewMyTickets(const Json::Value &req);
        Json::Value cancelTicket(const Json::Value &req);

        shanchuan::ConnectionPool *pool_;
        SessionManager *sessions_;
        UserRepository userRepo_;
        TicketRepository ticketRepo_;
        ReservationRepository resvRepo_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_TICKET_SERVICE_HPP
