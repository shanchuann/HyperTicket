#include "../../include/service/TicketService.hpp"

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    // ========== Admin Token ==========

    std::string TicketService::createAdminToken(const std::string &username,
                                                 bool mustChangePassword)
    {
        return sessions_->createAdmin(username, mustChangePassword, nowMs());
    }

    bool TicketService::resolveAdminToken(const std::string &token, std::string &usernameOut,
                                           bool &mustChangePasswordOut)
    {
        return sessions_->resolveAdmin(token, nowMs(), usernameOut, mustChangePasswordOut);
    }

    bool TicketService::authorizeAdmin(const Json::Value &req, std::string &usernameOut,
                                        std::string &errorOut, bool allowPasswordChangeOnly)
    {
        bool mustChangePassword = false;
        if (!resolveAdminToken(req.get(field::kAdminToken, "").asString(),
                               usernameOut, mustChangePassword))
        {
            errorOut = err::kAdminUnauthorized;
            return false;
        }
        if (!allowPasswordChangeOnly && mustChangePassword)
        {
            errorOut = "ADMIN_PASSWORD_CHANGE_REQUIRED";
            return false;
        }
        return true;
    }

    // ========== ADMIN_LOGIN (type 8) ==========
    // Request:  {"type":8,"username":"admin","passward":"xxx"}
    // Response: {"status":"OK","admin_token":"adm_...","username":"admin","role":"superadmin"}

    Json::Value TicketService::adminLogin(const Json::Value &req)
    {
        std::string username = req.get(field::kUserName, "").asString();
        std::string passwd   = req.get(field::kPassword, "").asString();
        if (username.empty() || passwd.empty())
            return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        const std::string ip = req.get("_client_ip", "").asString();
        const std::string device = req.get("client_id", "").asString().substr(0, 128);
        int64_t retryAfter = 0;
        if (!authRepo_.isBlocked(conn, "admin_account", username, retryAfter))
            return makeError(err::kDbUnavailable);
        if (retryAfter == 0 && !ip.empty() && !authRepo_.isBlocked(conn, "ip", ip, retryAfter))
            return makeError(err::kDbUnavailable);
        if (retryAfter == 0 && !device.empty() && !authRepo_.isBlocked(conn, "device", device, retryAfter))
            return makeError(err::kDbUnavailable);
        if (retryAfter > 0)
        {
            Json::Value locked = makeError("AUTH_TEMPORARILY_LOCKED");
            locked["retry_after_seconds"] = static_cast<Json::Int64>(retryAfter);
            return locked;
        }

        Admin admin;
        if (!adminRepo_.findByUsername(conn, username, admin))
        {
            authRepo_.recordFailure(conn, "admin_account", username, authFailureWindowSeconds_, authMaxFailures_, authLockSeconds_);
            if (!ip.empty()) authRepo_.recordFailure(conn, "ip", ip, authFailureWindowSeconds_, authMaxFailures_ * 4, authLockSeconds_);
            if (!device.empty()) authRepo_.recordFailure(conn, "device", device, authFailureWindowSeconds_, authMaxFailures_ * 2, authLockSeconds_);
            authRepo_.audit(conn, "admin", username, "LOGIN_FAILURE", ip, "invalid_credentials");
            return makeError(err::kAdminInvalidCredentials);
        }

        bool needRehash = false;
        if (!verifyPassword(passwd, admin.passwordHash, needRehash))
        {
            authRepo_.recordFailure(conn, "admin_account", username, authFailureWindowSeconds_, authMaxFailures_, authLockSeconds_);
            if (!ip.empty()) authRepo_.recordFailure(conn, "ip", ip, authFailureWindowSeconds_, authMaxFailures_ * 4, authLockSeconds_);
            if (!device.empty()) authRepo_.recordFailure(conn, "device", device, authFailureWindowSeconds_, authMaxFailures_ * 2, authLockSeconds_);
            authRepo_.audit(conn, "admin", username, "LOGIN_FAILURE", ip, "invalid_credentials");
            return makeError(err::kAdminInvalidCredentials);
        }

        if (needRehash)
            adminRepo_.updatePasswordHash(conn, username, hashPassword(passwd));

        // 检测是否仍使用默认密码 "password"
        bool dummy2 = false;
        bool isDefault = verifyPassword("password", admin.passwordHash, dummy2);

        std::string token = createAdminToken(username, isDefault);
        if (token.empty()) return makeError("SESSION_UNAVAILABLE");

        adminRepo_.recordLoginSuccess(conn, username);
        authRepo_.clear(conn, "admin_account", username);
        if (!device.empty()) authRepo_.clear(conn, "device", device);
        authRepo_.audit(conn, "admin", username, "LOGIN_SUCCESS", ip,
                        isDefault ? "default_password" : "");

        Json::Value res = makeOk();
        res[field::kAdminToken]      = token;
        res[field::kUserName]        = admin.username;
        res["role"]                  = admin.role;
        res["is_default_password"]   = isDefault;
        return res;
    }

    // ========== ADMIN_CHANGE_PASSWORD (type 15) ==========
    // Request:  {"type":15,"admin_token":"adm_...","new_password":"xxx"}
    // Response: {"status":"OK"}

    Json::Value TicketService::adminChangePassword(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError, true))
            return makeError(authError);

        std::string newPwd = req.get("new_password", "").asString();
        if (!isStrongPassword(newPwd))
            return makeError(err::kPasswordTooWeak);

        // 不允许与默认密码相同
        bool dummy = false;
        if (verifyPassword(newPwd, hashPassword("password"), dummy))
            return makeError(err::kPasswordSameAsOld);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        const std::string newHash = hashPassword(newPwd);
        if (newHash.empty() || !adminRepo_.updatePasswordHash(conn, who, newHash))
            return makeError(err::kDbUpdate);

        // 写入成功不等于持久化内容正确；回读并验证后才向客户端确认。
        Admin updatedAdmin;
        bool needRehash = false;
        if (!adminRepo_.findByUsername(conn, who, updatedAdmin) ||
            !verifyPassword(newPwd, updatedAdmin.passwordHash, needRehash))
            return makeError(err::kDbUpdate);

        sessions_->removeAllForAdmin(who);
        authRepo_.audit(conn, "admin", who, "PASSWORD_CHANGED",
                        req.get("_client_ip", "").asString(), "all_sessions_revoked");
        LOG_INFO << "admin[" << who << "] changed password";
        return makeOk();
    }

    // ========== ADMIN_LIST_TICKETS (type 9) ==========
    // Request:  {"type":9,"admin_token":"adm_..."}
    // Response: {"status":"OK","num":N,"arr":[{ticket fields...}]}

    Json::Value TicketService::adminListTickets(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError))
            return makeError(authError);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Ticket> tickets = ticketRepo_.listAll(conn);
        Json::Value res = makeOk();
        res[field::kNum] = static_cast<int>(tickets.size());
        for (const Ticket &t : tickets)
        {
            Json::Value item;
            item["ticket_id"]      = static_cast<Json::Int64>(t.id);
            item[field::kTitle]    = t.title;
            item[field::kVenue]    = t.venue;
            item["total_seats"]    = t.totalSeats;
            item["available_seats"]= t.availableSeats;
            item[field::kEventDate]= t.eventDate;
            item["status"]         = t.status;
            item["cover_image"]    = t.coverImage;
            item["category"]       = t.category;
            item["price"]          = t.price;
            item["city"]           = t.city;
            item["artist"]         = t.artist;
            res[field::kArr].append(item);
        }
        return res;
    }

    // ========== ADMIN_ADD_TICKET (type 10) ==========
    // Request:  {"type":10,"admin_token":"adm_...","title":"xxx","venue":"xxx",
    //            "event_date":"YYYY-MM-DD","total_seats":100}
    // Response: {"status":"OK"}

    Json::Value TicketService::adminAddTicket(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError))
            return makeError(authError);

        std::string title     = req.get(field::kTitle, "").asString();
        std::string venue     = req.get(field::kVenue, "").asString();
        std::string eventDate = req.get(field::kEventDate, "").asString();
        int totalSeats        = static_cast<int>(getIntField(req, field::kTotalSeats, 0));
        std::string coverImage = req.get("cover_image", "").asString();
        std::string category  = req.get("category", "concert").asString();
        int price             = static_cast<int>(getIntField(req, "price", 0));
        std::string city      = req.get(field::kCity, "北京").asString();
        std::string artist    = req.get(field::kArtist, "").asString();
        std::string description = req.get(field::kDescription, "").asString();
        std::string notice    = req.get(field::kNotice, "").asString();

        if (title.empty() || venue.empty() || totalSeats <= 0)
            return makeError(err::kInvalidInput);
        if (eventDate.size() != 10 || eventDate[4] != '-' || eventDate[7] != '-')
            return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        if (!ticketRepo_.insert(conn, title, venue, totalSeats, eventDate, coverImage,
                                category, price, city, artist, description, notice))
            return makeError(err::kDbInsert);

        int64_t ticketId = ticketRepo_.lastInsertId(conn);
        // 自动生成座位（非阻塞，插入失败不影响票务创建成功）
        seatRepo_.generate(conn, ticketId, totalSeats, price);

        stock_->set(ticketId, totalSeats);   // 预热库存缓存
        stock_->invalidateTicketList();      // 在售列表已变化

        LOG_INFO << "admin[" << who << "] added ticket: " << title;
        return makeOk();
    }

    // ========== ADMIN_DELETE_TICKET (type 11) ==========
    // Request:  {"type":11,"admin_token":"adm_...","ticket_id":1}
    // Response: {"status":"OK"}

    Json::Value TicketService::adminDeleteTicket(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError))
            return makeError(authError);

        if (!req.isMember(field::kTicketId))
            return makeError(err::kInvalidInput);
        int64_t ticketId = getIntField(req, field::kTicketId, -1);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        if (!ticketRepo_.exists(conn, ticketId))
            return makeError(err::kTicketNotFound);
        if (!ticketRepo_.setOffline(conn, ticketId))
            return makeError(err::kDbUpdate);

        stock_->set(ticketId, 0);            // 下架票在缓存层直接秒拒
        stock_->invalidateTicketList();      // 在售列表已变化

        LOG_INFO << "admin[" << who << "] offlined ticket_id=" << ticketId;
        return makeOk();
    }

    // ========== ADMIN_LIST_USERS (type 12) ==========
    // Request:  {"type":12,"admin_token":"adm_..."}
    // Response: {"status":"OK","num":N,"arr":[{user fields...}]}

    Json::Value TicketService::adminListUsers(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError))
            return makeError(authError);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<User> users = userRepo_.listAll(conn);
        Json::Value res = makeOk();
        res[field::kNum] = static_cast<int>(users.size());
        for (const User &u : users)
        {
            Json::Value item;
            item["user_id"]  = static_cast<Json::Int64>(u.id);
            item["tel"]      = u.tel;
            item["username"] = u.username;
            item["status"]   = u.status; // 1正常 0黑名单
            res[field::kArr].append(item);
        }
        return res;
    }

    // ========== ADMIN_STATS (type 13) ==========
    // Request:  {"type":13,"admin_token":"adm_..."}
    // Response: {"status":"OK","user_count":N,"ticket_count":N,"order_count":N,"today_orders":N}

    Json::Value TicketService::adminStats(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError))
            return makeError(authError);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        const char *sql =
            "SELECT "
            "(SELECT COUNT(*) FROM users), "
            "(SELECT COUNT(*) FROM tickets WHERE status=1), "
            "(SELECT COUNT(*) FROM reservations), "
            "(SELECT COUNT(*) FROM reservations WHERE DATE(created_at) = CURDATE())";

        if (mysql_query(conn, sql) != 0)
        {
            LOG_ERROR << "adminStats query failed: " << mysql_error(conn);
            return makeError(err::kDbQuery);
        }
        MYSQL_RES *result = mysql_store_result(conn);
        if (!result) return makeError(err::kDbResult);

        MYSQL_ROW row = mysql_fetch_row(result);
        Json::Value res = makeOk();
        if (row)
        {
            res["user_count"]   = row[0] ? std::stoi(row[0]) : 0;
            res["ticket_count"] = row[1] ? std::stoi(row[1]) : 0;
            res["order_count"]  = row[2] ? std::stoi(row[2]) : 0;
            res["today_orders"] = row[3] ? std::stoi(row[3]) : 0;
        }
        mysql_free_result(result);
        return res;
    }

    // ========== ADMIN_BLACKLIST (type 14) ==========
    // Request:  {"type":14,"admin_token":"adm_...","tel":"13800138000","action":"add"|"remove"}
    // Response: {"status":"OK"}

    Json::Value TicketService::adminBlacklist(const Json::Value &req)
    {
        std::string who;
        std::string authError;
        if (!authorizeAdmin(req, who, authError))
            return makeError(authError);

        std::string tel    = req.get(field::kTel, "").asString();
        std::string action = req.get(field::kAction, "").asString();
        if (tel.empty() || (action != "add" && action != "remove"))
            return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        int fromStatus = (action == "add") ? 1 : 0;
        int toStatus   = (action == "add") ? 0 : 1;
        int r = userRepo_.compareAndSetStatus(conn, tel, fromStatus, toStatus);
        if (r < 0) return makeError(err::kDbUpdate);
        if (r == 0)
        {
            int cur = 1;
            if (!userRepo_.getStatusByTel(conn, tel, cur))
                return makeError(err::kUserNotFound);
            return makeError(err::kAlreadyInState);
        }

        LOG_INFO << "admin[" << who << "] blacklist " << action << " tel=" << tel;
        return makeOk();
    }

} // namespace hyperticket
