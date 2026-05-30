#include "ser.hpp"

#include "../../Common/include/AppConfig.hpp"
#include "../include/SessionManager.hpp"
#include "../include/MysqlStmt.hpp"
#include "../../SqlConnPool/include/ConnectionPool.hpp"
#include "../../ChronoLite/include/AsynLogging.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include "../../ChronoLite/include/Timestamp.hpp"
#include "../../FixedThreadPool/include/FixedThreadPool.hpp"
#include "../../Inet/include/Buffer.hpp"
#include "../../Inet/include/EventLoop.hpp"
#include "../../Inet/include/InetAddress.hpp"
#include "../../Inet/include/TcpServer.hpp"
#include "../../ScheduledThreadPool/include/ScheduledThreadPool.hpp"

#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>

#include <atomic>
#include <any>
#include <sstream>
#include <algorithm>
#include <iomanip>

using shanchuan::Buffer;
using shanchuan::TcpConnectionPtr;

namespace
{
    // 当前时间（毫秒），用于 SessionManager 的 TTL 计算。
    int64_t nowMs()
    {
        return logsys::Timestamp::Now().getMicroSec() / 1000;
    }

    // 单连接令牌桶限流器，存放于 TcpConnection 的 context 中。
    // 同一连接的回调始终在同一 IO 线程执行（one loop per thread），故无需加锁。
    struct RateLimiter
    {
        double tokens;        // 当前令牌数
        double capacity;      // 桶容量 = 每秒最大请求数
        double refillPerMs;   // 每毫秒补充的令牌
        int64_t lastMs;       // 上次补充时间

        explicit RateLimiter(int perSec)
            : tokens(perSec), capacity(perSec),
              refillPerMs(perSec / 1000.0), lastMs(nowMs()) {}

        // 尝试消费一个令牌；无令牌返回 false（应丢弃/拒绝该请求）。
        bool allow()
        {
            int64_t now = nowMs();
            tokens += (now - lastMs) * refillPerMs;
            if (tokens > capacity) tokens = capacity;
            lastMs = now;
            if (tokens >= 1.0)
            {
                tokens -= 1.0;
                return true;
            }
            return false;
        }
    };

    std::string hashPassword(const std::string &password)
    {
        // FNV-1a 64-bit for deterministic password hashing in this codebase.
        uint64_t hash = 1469598103934665603ULL;
        for (unsigned char ch : password)
        {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        std::ostringstream oss;
        oss << std::hex << hash;
        return oss.str();
    }

    logsys::LOG_LEVEL parseLogLevel(const std::string &level)
    {
        if (level == "TRACE") return logsys::LOG_LEVEL::TRACE;
        if (level == "DEBUG") return logsys::LOG_LEVEL::DEBUG;
        if (level == "WARN") return logsys::LOG_LEVEL::WARN;
        if (level == "ERROR") return logsys::LOG_LEVEL::ERROR;
        if (level == "FATAL") return logsys::LOG_LEVEL::FATAL;
        return logsys::LOG_LEVEL::INFO;
    }

    bool execSql(MYSQL *conn, const std::string &sql)
    {
        return mysql_query(conn, sql.c_str()) == 0;
    }

    bool ensureDatabaseAndTables(const hyperticket::DbConfig &db)
    {
        MYSQL *conn = mysql_init(nullptr);
        if (!conn)
        {
            LOG_ERROR << "mysql_init failed";
            return false;
        }

        if (!mysql_real_connect(conn, db.host.c_str(), db.user.c_str(), db.password.c_str(), nullptr, db.port, nullptr, 0))
        {
            LOG_ERROR << "mysql_real_connect failed: " << mysql_error(conn);
            mysql_close(conn);
            return false;
        }

        std::string createDb = "CREATE DATABASE IF NOT EXISTS " + db.name + " CHARACTER SET utf8mb4";
        if (!execSql(conn, createDb))
        {
            LOG_ERROR << "create database failed: " << mysql_error(conn);
            mysql_close(conn);
            return false;
        }

        if (!execSql(conn, "USE " + db.name))
        {
            LOG_ERROR << "use database failed: " << mysql_error(conn);
            mysql_close(conn);
            return false;
        }

        // Do not attempt to create database schema automatically on startup.
        // The environment operator should run db/init.sql manually to initialize schema.
        // Here we only ensure the configured database can be used.
        // If the database does not exist or the user lacks privileges, return error.
        // Note: caller can decide to abort startup if database not ready.

        mysql_close(conn);
        return true;
    }

    Json::Value makeError(const std::string &reason)
    {
        Json::Value res;
        res["status"] = "ERR";
        res["reason"] = reason;
        return res;
    }

    Json::Value makeOk()
    {
        Json::Value res;
        res["status"] = "OK";
        return res;
    }

    std::string toJsonLine(const Json::Value &value)
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string out = Json::writeString(builder, value);
        out.push_back('\n');
        return out;
    }

    void sendJson(const TcpConnectionPtr &conn, const Json::Value &value)
    {
        conn->send(toJsonLine(value));
    }

    class TicketService
    {
    public:
        TicketService(shanchuan::ConnectionPool *pool, hyperticket::SessionManager *sessions)
            : pool_(pool), sessions_(sessions) {}

        Json::Value handleRequest(const Json::Value &req)
        {
            int type = req.get("type", 0).asInt();
            switch (type)
            {
            case LOGIN:
                return login(req);
            case REGISTER:
                return reg(req);
            case EXIT:
                return makeOk();
            case VIEW:
                return viewTickets();
            case ORDER:
                return orderTicket(req);
            case VIEW_MY:
                return viewMyTickets(req);
            case CANCEL:
                return cancelTicket(req);
            default:
                return makeError("UNKNOWN_TYPE");
            }
        }

        bool refreshTicketStatus()
        {
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return false;
            const char *sql = "UPDATE tickets SET status = 0 WHERE event_date < CURDATE() AND status != 0";
            if (mysql_query(conn, sql) != 0)
            {
                LOG_ERROR << "refreshTicketStatus failed: " << mysql_error(conn);
                return false;
            }
            return true;
        }

        bool logStats()
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

    private:
        shanchuan::ConnectionPool *pool_;
        hyperticket::SessionManager *sessions_;

        Json::Value login(const Json::Value &req)
        {
            std::string tel = req.get("usertel", "").asString();
            std::string passwd = req.get("passward", "").asString();
            if (tel.size() != 11 || passwd.size() < 6 || passwd.size() > 16)
            {
                return makeError("INVALID_INPUT");
            }

            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            hyperticket::MysqlStmt st(conn, "SELECT id, username, password_hash, status FROM users WHERE tel = ?");
            if (!st.ok()) return makeError("DB_QUERY");
            st.bindString(0, tel);
            if (!st.execute() || !st.bindResults(4))
            {
                return makeError("DB_QUERY");
            }
            if (!st.fetch())
            {
                return makeError("USER_NOT_FOUND");
            }
            int64_t userId = st.getInt(0);
            std::string username = st.getString(1);
            std::string dbPass = st.getString(2);
            std::string status = st.getString(3);
            if (status != "1")
            {
                return makeError("BLACKLISTED");
            }
            if (dbPass != hashPassword(passwd))
            {
                return makeError("PASSWD_ERROR");
            }
            Json::Value resval = makeOk();
            resval["username"] = username;
            // 签发会话 token：后续 order/cancel/viewMy 凭此识别用户，不再信任客户端自报 tel。
            resval["token"] = sessions_->create(tel, userId, nowMs());
            return resval;
        }

        Json::Value reg(const Json::Value &req)
        {
            std::string tel = req.get("usertel", "").asString();
            std::string passwd = req.get("passward", "").asString();
            std::string name = req.get("username", "").asString();
            if (tel.size() != 11 || passwd.size() < 6 || passwd.size() > 16 || name.empty())
            {
                return makeError("INVALID_INPUT");
            }

            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            hyperticket::MysqlStmt st(conn, "INSERT INTO users (tel, username, password_hash, status) VALUES(?,?,?,1)");
            if (!st.ok()) return makeError("DB_INSERT");
            st.bindString(0, tel);
            st.bindString(1, name);
            st.bindString(2, hashPassword(passwd));
            if (!st.execute())
            {
                return makeError("DB_INSERT");
            }
            return makeOk();
        }

        Json::Value viewTickets()
        {
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            const char *sql = "SELECT id, title, venue, total_seats, available_seats, event_date, status FROM tickets WHERE status = 1";
            if (mysql_query(conn, sql) != 0)
            {
                return makeError("DB_QUERY");
            }

            MYSQL_RES *res = mysql_store_result(conn);
            if (!res) return makeError("DB_RESULT");

            int num = mysql_num_rows(res);
            Json::Value resval = makeOk();
            resval["num"] = num;
            for (int i = 0; i < num; ++i)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                Json::Value tmp;
                tmp["tk_id"] = row[0] ? row[0] : "";
                tmp["title"] = row[1] ? row[1] : "";
                tmp["addr"] = row[2] ? row[2] : "";
                tmp["max"] = row[3] ? row[3] : "";
                tmp["num"] = row[4] ? row[4] : "";
                tmp["use_date"] = row[5] ? row[5] : "";
                tmp["status"] = row[6] ? row[6] : "";
                resval["arr"].append(tmp);
            }
            mysql_free_result(res);
            return resval;
        }

        Json::Value orderTicket(const Json::Value &req)
        {
            int tk_id = req.get("index", -1).asInt();
            // 身份来自 token，而非客户端自报的 tel，杜绝越权下单。
            std::string tel;
            int64_t sessionUserId = 0;
            if (!sessions_->resolve(req.get("token", "").asString(), nowMs(), tel, sessionUserId))
            {
                return makeError("UNAUTHORIZED");
            }
            if (tk_id <= 0)
            {
                return makeError("INVALID_INPUT");
            }
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            if (mysql_query(conn, "BEGIN") != 0)
            {
                return makeError("DB_BEGIN");
            }

            int user_id = static_cast<int>(sessionUserId);

            // Lock the ticket row for update to prevent oversell
            std::string s1 = "SELECT available_seats, status FROM tickets WHERE id = " + std::to_string(tk_id) + " FOR UPDATE";
            if (mysql_query(conn, s1.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_QUERY");
            }
            MYSQL_RES *res = mysql_store_result(conn);
            if (!res || mysql_num_rows(res) != 1)
            {
                if (res) mysql_free_result(res);
                mysql_query(conn, "ROLLBACK");
                return makeError("TICKET_NOT_FOUND");
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            int available = row[0] ? atoi(row[0]) : 0;
            int status = row[1] ? atoi(row[1]) : 0;
            mysql_free_result(res);
            if (status != 1)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("TICKET_OFFLINE");
            }
            if (available <= 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("NO_TICKET");
            }

            std::string s2 = "UPDATE tickets SET available_seats = available_seats - 1 WHERE id = " + std::to_string(tk_id);
            if (mysql_query(conn, s2.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_UPDATE");
            }

            // Insert reservation (soft-state managed)
            std::string s3 = "INSERT INTO reservations (user_id, ticket_id, quantity, status) VALUES(" + std::to_string(user_id) + ", " + std::to_string(tk_id) + ", 1, 'CONFIRMED')";
            if (mysql_query(conn, s3.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_INSERT");
            }

            // Insert audit record (parameterized; tel comes from the verified session)
            {
                hyperticket::MysqlStmt sa(conn, "INSERT INTO reservation_audit (reservation_id, action, detail) VALUES(LAST_INSERT_ID(), 'CREATE', ?)");
                if (sa.ok())
                {
                    sa.bindString(0, "user:" + tel);
                    sa.execute(); // best-effort: ignore audit errors
                }
            }

            mysql_query(conn, "COMMIT");
            return makeOk();
        }

        Json::Value viewMyTickets(const Json::Value &req)
        {
            std::string tel;
            int64_t userId = 0;
            if (!sessions_->resolve(req.get("token", "").asString(), nowMs(), tel, userId))
            {
                return makeError("UNAUTHORIZED");
            }
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            // Join reservations with users and tickets, exclude cancelled
            hyperticket::MysqlStmt st(conn,
                "SELECT r.id, r.ticket_id, t.title, t.venue, t.event_date, r.status "
                "FROM reservations r JOIN users u ON r.user_id = u.id "
                "JOIN tickets t ON r.ticket_id = t.id "
                "WHERE u.tel = ? AND r.status != 'CANCELLED'");
            if (!st.ok()) return makeError("DB_QUERY");
            st.bindString(0, tel);
            if (!st.execute() || !st.bindResults(6))
            {
                return makeError("DB_QUERY");
            }

            Json::Value resval = makeOk();
            int num = 0;
            while (st.fetch())
            {
                Json::Value tmp;
                tmp["id"] = st.getString(0);
                tmp["tk_id"] = st.getString(1);
                tmp["title"] = st.getString(2);
                tmp["addr"] = st.getString(3);
                tmp["use_date"] = st.getString(4);
                tmp["event_date"] = st.getString(4);
                tmp["status"] = st.getString(5);
                resval["arr"].append(tmp);
                ++num;
            }
            resval["num"] = num;
            return resval;
        }

        Json::Value cancelTicket(const Json::Value &req)
        {
            int index = req.get("index", -1).asInt();
            std::string tel;
            int64_t userId = 0;
            if (!sessions_->resolve(req.get("token", "").asString(), nowMs(), tel, userId))
            {
                return makeError("UNAUTHORIZED");
            }
            if (index <= 0)
            {
                return makeError("INVALID_INPUT");
            }
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            if (mysql_query(conn, "BEGIN") != 0)
            {
                return makeError("DB_BEGIN");
            }

            // find reservation and ensure ownership (user_id from verified session)
            int res_id = 0, tk_id = 0, qty = 1;
            std::string rstatus;
            {
                hyperticket::MysqlStmt sf(conn,
                    "SELECT r.id, r.ticket_id, r.quantity, r.status FROM reservations r "
                    "WHERE r.id = ? AND r.user_id = ? FOR UPDATE");
                if (!sf.ok())
                {
                    mysql_query(conn, "ROLLBACK");
                    return makeError("DB_QUERY");
                }
                sf.bindInt(0, index);
                sf.bindInt(1, userId);
                if (!sf.execute() || !sf.bindResults(4))
                {
                    mysql_query(conn, "ROLLBACK");
                    return makeError("DB_QUERY");
                }
                if (!sf.fetch())
                {
                    mysql_query(conn, "ROLLBACK");
                    return makeError("ORDER_NOT_FOUND");
                }
                res_id = static_cast<int>(sf.getInt(0));
                tk_id = static_cast<int>(sf.getInt(1));
                qty = static_cast<int>(sf.getInt(2));
                rstatus = sf.getString(3);
            }

            if (rstatus != "CONFIRMED")
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("ORDER_CANNOT_CANCEL");
            }

            // mark reservation as cancelled (soft delete)
            std::string sCancel = "UPDATE reservations SET status = 'CANCELLED', updated_at = NOW() WHERE id = " + std::to_string(res_id);
            if (mysql_query(conn, sCancel.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_UPDATE");
            }

            // restore inventory
            std::string sRestore = "UPDATE tickets SET available_seats = available_seats + " + std::to_string(qty) + " WHERE id = " + std::to_string(tk_id);
            if (mysql_query(conn, sRestore.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_UPDATE");
            }

            // audit (parameterized; tel from verified session)
            {
                hyperticket::MysqlStmt sa(conn, "INSERT INTO reservation_audit (reservation_id, action, detail) VALUES(?, 'CANCEL', ?)");
                if (sa.ok())
                {
                    sa.bindInt(0, res_id);
                    sa.bindString(1, "user:" + tel);
                    sa.execute();
                }
            }

            mysql_query(conn, "COMMIT");
            return makeOk();
        }
    };
} // namespace

int main()
{
    std::string configError;
    hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json", &configError);

    logsys::AsynLogging asyncLogger(cfg.log.basename, cfg.log.roll_size, cfg.log.flush_interval);
    logsys::Logger::SetOuput([&asyncLogger](const std::string &msg) { asyncLogger.append(msg); });
    logsys::Logger::SetFlush([&asyncLogger]() { asyncLogger.flush(); });
    logsys::Logger::SetLogLevel(parseLogLevel(cfg.log.level));
    asyncLogger.start();

    if (!configError.empty())
    {
        LOG_FATAL << "config load failed: " << configError;
        return 1;
    }

    if (!ensureDatabaseAndTables(cfg.db))
    {
        LOG_FATAL << "database init failed";
        return 1;
    }

    shanchuan::ConnectionPool *pool = shanchuan::ConnectionPool::GetInstance();
    pool->init(cfg.db.host, cfg.db.user, cfg.db.password, cfg.db.name, cfg.db.port, cfg.db.pool_size, 0);

    shanchuan::FixedThreadPool workerPool(cfg.server.worker_threads);
    shanchuan::ScheduledThreadPool scheduler;
    hyperticket::SessionManager sessions;
    TicketService service(pool, &sessions);

    shanchuan::EventLoop loop;
    shanchuan::InetAddress listenAddr(cfg.server.ip, static_cast<uint16_t>(cfg.server.port));
    shanchuan::TcpServer server(&loop, listenAddr, "HyperTicket");
    server.setThreadNum(cfg.server.io_threads);

    std::atomic<int> connCount{0};
    const int maxConn = cfg.server.max_connections;
    const int maxRps = cfg.server.max_requests_per_sec;

    server.setConnectionCallback([&connCount, maxConn, maxRps](const TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            int n = ++connCount;
            if (n > maxConn)
            {
                LOG_WARN << "connection limit reached (" << maxConn << "), refusing " << conn->peerAddress().toIp();
                --connCount;
                conn->forceClose();
                return;
            }
            // 为该连接安装独立的令牌桶限流器
            conn->setContext(RateLimiter(maxRps));
            LOG_INFO << "connection " << conn->name() << " UP (" << n << "/" << maxConn << ")";
        }
        else
        {
            --connCount;
            LOG_INFO << "connection " << conn->name() << " DOWN";
        }
    });

    server.setMessageCallback([&workerPool, &service](const TcpConnectionPtr &conn, Buffer *buf, shanchuan::Timestamp) {
        while (true)
        {
            const char *eol = buf->findEOL();
            if (!eol)
            {
                break;
            }
            std::string line(buf->peek(), static_cast<size_t>(eol - buf->peek()));
            buf->retrieveUntil(eol + 1);
            if (line.empty())
            {
                continue;
            }

            // 单连接限流：超过每秒请求上限则拒绝该请求。
            std::any *ctx = conn->getMutableContext();
            if (ctx && ctx->has_value())
            {
                RateLimiter *rl = std::any_cast<RateLimiter>(ctx);
                if (rl && !rl->allow())
                {
                    sendJson(conn, makeError("RATE_LIMITED"));
                    continue;
                }
            }

            Json::Value req;
            Json::CharReaderBuilder builder;
            builder["collectComments"] = false;
            std::string errs;
            std::istringstream iss(line);
            if (!Json::parseFromStream(builder, iss, &req, &errs))
            {
                Json::Value res = makeError("JSON_PARSE");
                sendJson(conn, res);
                continue;
            }

            workerPool.add_task([conn, req, &service]() {
                Json::Value res = service.handleRequest(req);
                sendJson(conn, res);
            });
        }
    });

    scheduler.addRunEvery(cfg.schedule.stats_interval_ms, [&service]() {
        service.logStats();
    });
    scheduler.addRunEvery(cfg.schedule.ticket_status_interval_ms, [&service]() {
        service.refreshTicketStatus();
    });
    // 定时清理过期会话 token，避免内存无限增长。
    scheduler.addRunEvery(60000, [&sessions]() {
        sessions.purgeExpired(nowMs());
    });

    server.start();
    LOG_INFO << "server started at " << cfg.server.ip << ":" << cfg.server.port;
    loop.loop();
    return 0;
}
