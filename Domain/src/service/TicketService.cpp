#include "../../include/service/TicketService.hpp"

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    Json::Value TicketService::handle(const Json::Value &req)
    {
        int type = req.get(field::kType, 0).asInt();
        switch (type)
        {
        case LOGIN:    return login(req);
        case REGISTER: return reg(req);
        case EXIT:     return makeOk();
        case VIEW:     return viewTickets();
        case ORDER:    return orderTicket(req);
        case VIEW_MY:  return viewMyTickets(req);
        case CANCEL:   return cancelTicket(req);
        default:       return makeError(err::kUnknownType);
        }
    }

    Json::Value TicketService::login(const Json::Value &req)
    {
        std::string tel = req.get(field::kUserTel, "").asString();
        std::string passwd = req.get(field::kPassword, "").asString();
        if (tel.size() != 11 || passwd.size() < 6 || passwd.size() > 16)
        {
            return makeError(err::kInvalidInput);
        }

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        User u;
        if (!userRepo_.findByTel(conn, tel, u))
        {
            return makeError(err::kInvalidCredentials);
        }
        if (u.status != 1)
        {
            return makeError(err::kBlacklisted);
        }
        bool needRehash = false;
        if (!verifyPassword(passwd, u.passwordHash, needRehash))
        {
            return makeError(err::kInvalidCredentials);
        }
        // 旧 FNV-1a 哈希自动升级为 bcrypt
        if (needRehash)
        {
            userRepo_.updatePasswordHash(conn, tel, hashPassword(passwd));
        }
        Json::Value res = makeOk();
        res[field::kUserName] = u.username;
        // 签发会话 token：后续 order/cancel/viewMy 凭此识别用户，不信任客户端自报 tel。
        res[field::kToken] = sessions_->create(tel, u.id, nowMs());
        return res;
    }

    // 密码强度校验：6-16 位，须包含数字、小写字母、大写字母。
    static bool isPasswordStrong(const std::string &pwd)
    {
        if (pwd.size() < 6 || pwd.size() > 16) return false;
        bool hasDigit = false, hasLower = false, hasUpper = false;
        for (unsigned char ch : pwd)
        {
            if (ch >= '0' && ch <= '9') hasDigit = true;
            else if (ch >= 'a' && ch <= 'z') hasLower = true;
            else if (ch >= 'A' && ch <= 'Z') hasUpper = true;
        }
        return hasDigit && hasLower && hasUpper;
    }

    Json::Value TicketService::reg(const Json::Value &req)
    {
        std::string tel = req.get(field::kUserTel, "").asString();
        std::string passwd = req.get(field::kPassword, "").asString();
        std::string name = req.get(field::kUserName, "").asString();
        if (tel.size() != 11 || name.empty())
        {
            return makeError(err::kInvalidInput);
        }
        if (!isPasswordStrong(passwd))
        {
            return makeError(err::kWeakPassword);
        }

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        if (!userRepo_.insert(conn, tel, name, hashPassword(passwd)))
        {
            return makeError(err::kDbInsert);
        }
        User u;
        if (!userRepo_.findByTel(conn, tel, u))
        {
            return makeError(err::kDbInsert); // 刚插入却查不到，视为失败
        }
        Json::Value res = makeOk();
        res[field::kUserName] = u.username;
        res[field::kToken] = sessions_->create(tel, u.id, nowMs());
        return res;
    }

    Json::Value TicketService::viewTickets()
    {
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Ticket> tickets = ticketRepo_.listOnSale(conn);
        Json::Value res = makeOk();
        res[field::kNum] = static_cast<int>(tickets.size());
        for (const Ticket &t : tickets)
        {
            Json::Value tmp;
            tmp["tk_id"] = std::to_string(t.id);
            tmp["title"] = t.title;
            tmp["addr"] = t.venue;
            tmp["max"] = std::to_string(t.totalSeats);
            tmp["num"] = std::to_string(t.availableSeats);
            tmp["use_date"] = t.eventDate;
            tmp["status"] = std::to_string(t.status);
            res[field::kArr].append(tmp);
        }
        return res;
    }

    bool TicketService::refreshTicketStatus()
    {
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return false;
        if (!ticketRepo_.offlineExpired(conn))
        {
            LOG_ERROR << "refreshTicketStatus failed: " << mysql_error(conn);
            return false;
        }
        return true;
    }

    bool TicketService::logStats()
    {
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return false;
        const char *sql = "SELECT (SELECT COUNT(*) FROM users), (SELECT COUNT(*) FROM tickets), (SELECT COUNT(*) FROM reservations)";
        if (mysql_query(conn, sql) != 0)
        {
            LOG_ERROR << "logStats failed: " << mysql_error(conn);
            return false;
        }
        MYSQL_RES *res = mysql_store_result(conn);
        if (!res) return false;
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            LOG_INFO << "stats users=" << row[0] << " tickets=" << row[1] << " reservations=" << row[2];
        }
        mysql_free_result(res);
        return true;
    }
} // namespace hyperticket
