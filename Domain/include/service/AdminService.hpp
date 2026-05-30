#ifndef HYPERTICKET_ADMIN_SERVICE_HPP
#define HYPERTICKET_ADMIN_SERVICE_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../repository/UserRepository.hpp"
#include "../repository/TicketRepository.hpp"

namespace hyperticket
{
    // 管理端业务逻辑层：纯数据操作，不含控制台 I/O（交互留在 admin.cpp）。
    // 持有裸 MYSQL*（admin 直连，不走连接池），通过 Repository 执行参数化 SQL，
    // 从而与 ser 共享同一套防注入的数据访问。
    //
    // 结果约定：返回 bool 表示操作成功与否；查询类返回实体向量。
    // 针对“需要区分用户不存在/已在黑名单”等情形，用枚举返回更精确的结果。
    enum class BlacklistResult
    {
        Ok,
        UserNotFound,
        AlreadyInState, // 加入时已在黑名单 / 移出时本就正常
        DbError,
    };

    class AdminService
    {
    public:
        explicit AdminService(MYSQL *conn) : conn_(conn) {}

        bool addTicket(const std::string &venue, int totalSeats, const std::string &eventDate)
        {
            return ticketRepo_.insert(conn_, venue, venue, totalSeats, eventDate);
        }

        std::vector<Ticket> listAllTickets() { return ticketRepo_.listAll(conn_); }

        // 下架票务；票不存在返回 false。
        bool deleteTicket(int64_t ticketId)
        {
            if (!ticketRepo_.exists(conn_, ticketId)) return false;
            return ticketRepo_.setOffline(conn_, ticketId);
        }

        std::vector<User> listAllUsers() { return userRepo_.listAll(conn_); }
        std::vector<User> listBlacklist() { return userRepo_.listByStatus(conn_, 0); }

        BlacklistResult addToBlacklist(const std::string &tel)
        {
            return changeStatus(tel, /*from=*/1, /*to=*/0);
        }
        BlacklistResult removeFromBlacklist(const std::string &tel)
        {
            return changeStatus(tel, /*from=*/0, /*to=*/1);
        }

    private:
        // 仅当当前状态==from 时改为 to；据此区分各种结果。
        BlacklistResult changeStatus(const std::string &tel, int from, int to)
        {
            int cur = 0;
            if (!userRepo_.getStatusByTel(conn_, tel, cur))
            {
                return BlacklistResult::UserNotFound;
            }
            if (cur != from)
            {
                return BlacklistResult::AlreadyInState;
            }
            if (!userRepo_.setStatusByTel(conn_, tel, to))
            {
                return BlacklistResult::DbError;
            }
            return BlacklistResult::Ok;
        }

        MYSQL *conn_;
        UserRepository userRepo_;
        TicketRepository ticketRepo_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_ADMIN_SERVICE_HPP
