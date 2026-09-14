#include "../../include/service/TicketService.hpp"

#include <sstream>

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    Json::Value TicketService::handle(const Json::Value &req)
    {
        int type = static_cast<int>(getIntField(req, field::kType, 0));
        switch (type)
        {
        case LOGIN:              return login(req);
        case REGISTER:           return reg(req);
        case EXIT:               return makeOk();
        case VIEW:               return viewTickets(req);
        case ORDER:              return orderTicket(req);
        case VIEW_MY:            return viewMyTickets(req);
        case CANCEL:             return cancelTicket(req);
        case ADMIN_LOGIN:        return adminLogin(req);
        case ADMIN_LIST_TICKETS: return adminListTickets(req);
        case ADMIN_ADD_TICKET:   return adminAddTicket(req);
        case ADMIN_DELETE_TICKET:return adminDeleteTicket(req);
        case ADMIN_LIST_USERS:   return adminListUsers(req);
        case ADMIN_STATS:           return adminStats(req);
        case ADMIN_BLACKLIST:       return adminBlacklist(req);
        case ADMIN_CHANGE_PASSWORD: return adminChangePassword(req);
        case DELETE_ORDER:          return deleteOrder(req);
        case VIEW_SEATS:            return viewSeats(req);
        case VERIFY_ORDER:          return verifyOrder(req);
        case TICKET_DETAIL:         return ticketDetail(req);
        case PAY_ORDER:             return payOrder(req);
        case PAY_QUERY:             return queryPayment(req);
        case ORDER_QUERY:           return queryQueuedOrder(req);
        case FAVORITE:              return favorite(req);
        case VIEW_FAVORITES:        return viewFavorites(req);
        case HOT_TICKETS:           return hotTickets(req);
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
        std::string token = sessions_->create(tel, u.id, nowMs());
        if (token.empty())
        {
            // Redis 故障导致签发失败：明确报错，不能返回空 token 的"假成功"
            return makeError("SESSION_UNAVAILABLE");
        }
        res[field::kToken] = token;
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
        std::string token = sessions_->create(tel, u.id, nowMs());
        if (token.empty())
        {
            return makeError("SESSION_UNAVAILABLE");
        }
        res[field::kToken] = token;
        return res;
    }

    // 票务列表统一序列化（列表/搜索/收藏/热门共用）。
    static void appendTicket(Json::Value &arr, const Ticket &t)
    {
        Json::Value tmp;
        tmp["tk_id"] = std::to_string(t.id);
        tmp["title"] = t.title;
        tmp["addr"] = t.venue;
        tmp["max"] = std::to_string(t.totalSeats);
        tmp["num"] = std::to_string(t.availableSeats);
        tmp["use_date"] = t.eventDate;
        tmp["status"] = std::to_string(t.status);
        tmp["cover_image"] = t.coverImage;
        tmp["category"] = t.category;
        tmp["price"] = t.price;
        tmp["city"] = t.city;
        tmp["artist"] = t.artist;
        arr.append(tmp);
    }

    Json::Value TicketService::viewTickets(const Json::Value &req)
    {
        std::string keyword  = req.get(field::kKeyword, "").asString();
        std::string city     = req.get(field::kCity, "").asString();
        std::string category = req.get(field::kCategory, "").asString();
        const bool filtered = !keyword.empty() || !city.empty() || !category.empty();

        // ── 读缓存：仅无过滤条件的全量列表走缓存（过滤组合太多，缓存命中率低）──
        if (!filtered)
        {
            std::string cached;
            if (stock_->getTicketList(cached))
            {
                Json::CharReaderBuilder rb;
                Json::Value res;
                std::istringstream iss(cached);
                std::string errs;
                if (Json::parseFromStream(rb, iss, &res, &errs))
                    return res;
                // 缓存内容损坏：忽略并走 DB 重建
            }
        }

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Ticket> tickets = filtered
            ? ticketRepo_.search(conn, keyword, city, category)
            : ticketRepo_.listOnSale(conn);
        Json::Value res = makeOk();
        res[field::kNum] = static_cast<int>(tickets.size());
        res[field::kArr] = Json::Value(Json::arrayValue);
        for (const Ticket &t : tickets)
            appendTicket(res[field::kArr], t);

        if (!filtered)
        {
            // 回填列表缓存（5s TTL：写操作会主动失效，TTL 只是兜底）
            // 顺带预热各票的库存 key，让下单预扣减尽量命中
            Json::StreamWriterBuilder wb;
            wb["indentation"] = "";
            stock_->setTicketList(Json::writeString(wb, res), 5);
            for (const Ticket &t : tickets)
                stock_->set(t.id, t.availableSeats);
        }
        return res;
    }

    // ========== TICKET_DETAIL (type 19) — 无需认证 ==========
    Json::Value TicketService::ticketDetail(const Json::Value &req)
    {
        int64_t tkId = getIntField(req, field::kIndex, -1);
        if (tkId <= 0) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Ticket t;
        if (!ticketRepo_.findDetail(conn, tkId, t))
            return makeError(err::kTicketNotFound);

        Json::Value res = makeOk();
        res["tk_id"] = std::to_string(t.id);
        res["title"] = t.title;
        res["addr"] = t.venue;
        res["max"] = std::to_string(t.totalSeats);
        res["num"] = std::to_string(t.availableSeats);
        res["use_date"] = t.eventDate;
        // 注意：不能写 res["status"]——会覆盖 makeOk() 的协议字段 "OK"，
        // 前端会误判为请求失败。票的上下架状态用 tk_status 传递。
        res["tk_status"] = std::to_string(t.status);
        res["cover_image"] = t.coverImage;
        res["category"] = t.category;
        res["price"] = t.price;
        res["city"] = t.city;
        res["artist"] = t.artist;
        res["description"] = t.description;
        res["notice"] = t.notice;
        return res;
    }

    // ========== HOT_TICKETS (type 23) — 无需认证 ==========
    Json::Value TicketService::hotTickets(const Json::Value &req)
    {
        int limit = static_cast<int>(getIntField(req, "limit", 10));
        if (limit <= 0 || limit > 50) limit = 10;

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Ticket> tickets = ticketRepo_.hotTickets(conn, limit);
        Json::Value res = makeOk();
        res[field::kNum] = static_cast<int>(tickets.size());
        res[field::kArr] = Json::Value(Json::arrayValue);
        for (const Ticket &t : tickets)
        {
            appendTicket(res[field::kArr], t);
            res[field::kArr][res[field::kArr].size() - 1]["hot"] = t.hotScore;
        }
        return res;
    }

    // ========== FAVORITE (type 21) ==========
    Json::Value TicketService::favorite(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        int64_t tkId = getIntField(req, field::kIndex, -1);
        std::string action = req.get(field::kAction, "add").asString();
        if (tkId <= 0 || (action != "add" && action != "remove"))
            return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        bool ok = (action == "add")
            ? (ticketRepo_.exists(conn, tkId) && favRepo_.add(conn, userId, tkId))
            : favRepo_.remove(conn, userId, tkId);
        if (!ok) return makeError(action == "add" ? err::kTicketNotFound : err::kDbUpdate);
        return makeOk();
    }

    // ========== VIEW_FAVORITES (type 22) ==========
    Json::Value TicketService::viewFavorites(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Ticket> tickets = favRepo_.listByUser(conn, userId);
        Json::Value res = makeOk();
        res[field::kNum] = static_cast<int>(tickets.size());
        res[field::kArr] = Json::Value(Json::arrayValue);
        for (const Ticket &t : tickets)
            appendTicket(res[field::kArr], t);
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
        stock_->invalidateTicketList(); // 过期票下架，列表缓存失效
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
