#ifndef HYPERTICKET_ADMIN_SERVICE_HPP
#define HYPERTICKET_ADMIN_SERVICE_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../ServiceUtil.hpp"
#include "../repository/AdminRepository.hpp"
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

        // 管理员登录：用户名 + 密码验证。
        bool login(const std::string &username, const std::string &password)
        {
            Admin admin;
            if (!adminRepo_.findByUsername(conn_, username, admin))
                return false;
            bool needRehash = false;
            if (!verifyPassword(password, admin.passwordHash, needRehash))
                return false;
            // 旧哈希自动迁移
            if (needRehash)
            {
                adminRepo_.updatePasswordHash(conn_, username, hashPassword(password));
            }
            loggedAdmin_ = admin;
            return true;
        }

        const std::string &adminUsername() const { return loggedAdmin_.username; }
        const std::string &adminRole() const { return loggedAdmin_.role; }

        // 检测是否使用默认密码 "password"。
        bool isDefaultPassword() const
        {
            bool dummy = false;
            return verifyPassword("password", loggedAdmin_.passwordHash, dummy);
        }

        // 修改管理员密码（含强度校验）。
        bool changePassword(const std::string &username, const std::string &newPassword)
        {
            if (newPassword.size() < 6 || newPassword.size() > 16) return false;
            bool hasDigit = false, hasLower = false, hasUpper = false;
            for (unsigned char ch : newPassword)
            {
                if (ch >= '0' && ch <= '9') hasDigit = true;
                else if (ch >= 'a' && ch <= 'z') hasLower = true;
                else if (ch >= 'A' && ch <= 'Z') hasUpper = true;
            }
            if (!hasDigit || !hasLower || !hasUpper) return false;
            return adminRepo_.updatePasswordHash(conn_, username, hashPassword(newPassword));
        }

        bool addTicket(const std::string &venue, int totalSeats, const std::string &eventDate)
        {
            if (totalSeats <= 0) return false;
            // 简单校验日期格式 YYYY-MM-DD
            if (eventDate.size() != 10 || eventDate[4] != '-' || eventDate[7] != '-') return false;
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
        // 原子状态切换：仅当当前状态==from 时改为 to，避免 TOCTOU。
        BlacklistResult changeStatus(const std::string &tel, int from, int to)
        {
            int r = userRepo_.compareAndSetStatus(conn_, tel, from, to);
            if (r < 0)  return BlacklistResult::DbError;
            if (r == 0)
            {
                // 状态不匹配：可能是用户不存在，也可能是状态已变
                int cur = 0;
                if (!userRepo_.getStatusByTel(conn_, tel, cur))
                    return BlacklistResult::UserNotFound;
                return BlacklistResult::AlreadyInState;
            }
            return BlacklistResult::Ok;
        }

        MYSQL *conn_;
        Admin loggedAdmin_;
        AdminRepository adminRepo_;
        UserRepository userRepo_;
        TicketRepository ticketRepo_;
    };
} // namespace hyperticket
#endif // HYPERTICKET_ADMIN_SERVICE_HPP
