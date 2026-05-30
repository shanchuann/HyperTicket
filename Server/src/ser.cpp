#include "ser.hpp"

#include "../../Common/include/AppConfig.hpp"
#include "../../SqlConnPool/include/ConnectionPool.hpp"
#include "../../ChronoLite/include/AsynLogging.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include "../../FixedThreadPool/include/FixedThreadPool.hpp"
#include "../../Inet/include/Buffer.hpp"
#include "../../Inet/include/EventLoop.hpp"
#include "../../Inet/include/InetAddress.hpp"
#include "../../Inet/include/TcpServer.hpp"
#include "../../ScheduledThreadPool/include/ScheduledThreadPool.hpp"

#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>

#include <atomic>
#include <sstream>
#include <algorithm>
#include <iomanip>

using shanchuan::Buffer;
using shanchuan::TcpConnectionPtr;

namespace
{
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
        explicit TicketService(shanchuan::ConnectionPool *pool) : pool_(pool) {}

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

            std::string sql = "SELECT username, password_hash, status FROM users WHERE tel = '" + tel + "'";
            if (mysql_query(conn, sql.c_str()) != 0)
            {
                return makeError("DB_QUERY");
            }

            MYSQL_RES *res = mysql_store_result(conn);
            if (!res) return makeError("DB_RESULT");
            if (mysql_num_rows(res) != 1)
            {
                mysql_free_result(res);
                return makeError("USER_NOT_FOUND");
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            std::string dbPass = row[1] ? row[1] : "";
            std::string status = row[2] ? row[2] : "0";
            if (status != "1")
            {
                mysql_free_result(res);
                return makeError("BLACKLISTED");
            }
            if (dbPass != hashPassword(passwd))
            {
                mysql_free_result(res);
                return makeError("PASSWD_ERROR");
            }
            Json::Value resval = makeOk();
            resval["username"] = row[0] ? row[0] : "";
            mysql_free_result(res);
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

            std::string sql = "INSERT INTO users (tel, username, password_hash, status) VALUES('" + tel + "','" + name + "','" + hashPassword(passwd) + "',1)";
            if (mysql_query(conn, sql.c_str()) != 0)
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
            std::string tel = req.get("tel", "").asString();
            if (tk_id <= 0 || tel.size() != 11)
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

            // find user id by tel
            std::string sUser = "SELECT id FROM users WHERE tel = '" + tel + "'";
            if (mysql_query(conn, sUser.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_QUERY");
            }
            MYSQL_RES *res = mysql_store_result(conn);
            if (!res || mysql_num_rows(res) != 1)
            {
                if (res) mysql_free_result(res);
                mysql_query(conn, "ROLLBACK");
                return makeError("USER_NOT_FOUND");
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            int user_id = row[0] ? atoi(row[0]) : 0;
            mysql_free_result(res);

            // Lock the ticket row for update to prevent oversell
            std::string s1 = "SELECT available_seats, status FROM tickets WHERE id = " + std::to_string(tk_id) + " FOR UPDATE";
            if (mysql_query(conn, s1.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_QUERY");
            }
            res = mysql_store_result(conn);
            if (!res || mysql_num_rows(res) != 1)
            {
                if (res) mysql_free_result(res);
                mysql_query(conn, "ROLLBACK");
                return makeError("TICKET_NOT_FOUND");
            }
            row = mysql_fetch_row(res);
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

            // Insert audit record
            std::string sAudit = "INSERT INTO reservation_audit (reservation_id, action, detail) VALUES(LAST_INSERT_ID(), 'CREATE', 'user:" + tel + "')";
            // best-effort: ignore audit errors
            mysql_query(conn, sAudit.c_str());

            mysql_query(conn, "COMMIT");
            return makeOk();
        }

        Json::Value viewMyTickets(const Json::Value &req)
        {
            std::string tel = req.get("tel", "").asString();
            if (tel.size() != 11)
            {
                return makeError("INVALID_INPUT");
            }
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return makeError("DB_UNAVAILABLE");

            // Join reservations with users and tickets, exclude cancelled
            std::string sql = "SELECT r.id, r.ticket_id, t.title, t.venue, t.event_date, r.status FROM reservations r JOIN users u ON r.user_id = u.id JOIN tickets t ON r.ticket_id = t.id WHERE u.tel = '" + tel + "' AND r.status != 'CANCELLED'";
            if (mysql_query(conn, sql.c_str()) != 0)
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
                tmp["id"] = row[0] ? row[0] : "";
                tmp["tk_id"] = row[1] ? row[1] : "";
                tmp["title"] = row[2] ? row[2] : "";
                tmp["addr"] = row[3] ? row[3] : "";
                tmp["use_date"] = row[4] ? row[4] : "";
                tmp["event_date"] = row[4] ? row[4] : "";
                tmp["status"] = row[5] ? row[5] : "";
                resval["arr"].append(tmp);
            }
            mysql_free_result(res);
            return resval;
        }

        Json::Value cancelTicket(const Json::Value &req)
        {
            int index = req.get("index", -1).asInt();
            std::string tel = req.get("tel", "").asString();
            if (index <= 0 || tel.size() != 11)
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

            // find reservation and ensure ownership
            std::string sFind = "SELECT r.id, r.ticket_id, r.quantity, r.status FROM reservations r JOIN users u ON r.user_id = u.id WHERE r.id = " + std::to_string(index) + " AND u.tel = '" + tel + "' FOR UPDATE";
            if (mysql_query(conn, sFind.c_str()) != 0)
            {
                mysql_query(conn, "ROLLBACK");
                return makeError("DB_QUERY");
            }
            MYSQL_RES *res = mysql_store_result(conn);
            if (!res || mysql_num_rows(res) != 1)
            {
                if (res) mysql_free_result(res);
                mysql_query(conn, "ROLLBACK");
                return makeError("ORDER_NOT_FOUND");
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            int res_id = row[0] ? atoi(row[0]) : 0;
            int tk_id = row[1] ? atoi(row[1]) : 0;
            int qty = row[2] ? atoi(row[2]) : 1;
            std::string rstatus = row[3] ? row[3] : "";
            mysql_free_result(res);

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

            // audit
            std::string sAudit = "INSERT INTO reservation_audit (reservation_id, action, detail) VALUES(" + std::to_string(res_id) + ", 'CANCEL', 'user:" + tel + "')";
            mysql_query(conn, sAudit.c_str());

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
    TicketService service(pool);

    shanchuan::EventLoop loop;
    shanchuan::InetAddress listenAddr(cfg.server.ip, static_cast<uint16_t>(cfg.server.port));
    shanchuan::TcpServer server(&loop, listenAddr, "HyperTicket");
    server.setThreadNum(cfg.server.io_threads);

    server.setConnectionCallback([](const TcpConnectionPtr &conn) {
        std::string state = conn->connected() ? "UP" : "DOWN";
        LOG_INFO << "connection " << conn->name() << " " << state;
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

    server.start();
    LOG_INFO << "server started at " << cfg.server.ip << ":" << cfg.server.port;
    loop.loop();
    return 0;
}
