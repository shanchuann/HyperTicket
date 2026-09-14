#ifndef HYPERTICKET_AUTH_SECURITY_REPOSITORY_HPP
#define HYPERTICKET_AUTH_SECURITY_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>

#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // MySQL-backed authentication throttle shared by all server instances.
    class AuthSecurityRepository
    {
    public:
        bool isBlocked(MYSQL *conn, const std::string &scope,
                       const std::string &subject, int64_t &retryAfterSeconds)
        {
            MysqlStmt st(conn,
                "SELECT GREATEST(0, IFNULL(TIMESTAMPDIFF(SECOND, NOW(3), locked_until), 0)) "
                "FROM auth_throttles WHERE scope = ? AND subject = ?");
            if (!st.ok()) return false;
            st.bindString(0, scope);
            st.bindString(1, subject);
            if (!st.execute() || !st.bindResults(1) || !st.fetch())
            {
                retryAfterSeconds = 0;
                return true;
            }
            retryAfterSeconds = st.getInt(0);
            return true;
        }

        bool recordFailure(MYSQL *conn, const std::string &scope,
                           const std::string &subject, int windowSeconds,
                           int maxFailures, int lockSeconds)
        {
            MysqlStmt st(conn,
                "INSERT INTO auth_throttles(scope, subject, failure_count, window_started_at, locked_until) "
                "VALUES(?,?,1,NOW(3),NULL) "
                "ON DUPLICATE KEY UPDATE "
                "locked_until = CASE "
                " WHEN locked_until > NOW(3) THEN locked_until "
                " WHEN window_started_at < DATE_SUB(NOW(3), INTERVAL ? SECOND) THEN NULL "
                " WHEN failure_count + 1 >= ? THEN DATE_ADD(NOW(3), INTERVAL ? SECOND) "
                " ELSE NULL END, "
                "failure_count = IF(window_started_at < DATE_SUB(NOW(3), INTERVAL ? SECOND), 1, failure_count + 1), "
                "window_started_at = IF(window_started_at < DATE_SUB(NOW(3), INTERVAL ? SECOND), NOW(3), window_started_at), "
                "updated_at = NOW(3)");
            if (!st.ok()) return false;
            st.bindString(0, scope);
            st.bindString(1, subject);
            st.bindInt(2, windowSeconds);
            st.bindInt(3, maxFailures);
            st.bindInt(4, lockSeconds);
            st.bindInt(5, windowSeconds);
            st.bindInt(6, windowSeconds);
            return st.execute();
        }

        bool clear(MYSQL *conn, const std::string &scope, const std::string &subject)
        {
            MysqlStmt st(conn, "DELETE FROM auth_throttles WHERE scope = ? AND subject = ?");
            if (!st.ok()) return false;
            st.bindString(0, scope);
            st.bindString(1, subject);
            return st.execute();
        }

        bool audit(MYSQL *conn, const std::string &actorType,
                   const std::string &actor, const std::string &event,
                   const std::string &ip, const std::string &detail)
        {
            MysqlStmt st(conn,
                "INSERT INTO security_audit(actor_type, actor, event, ip_address, detail) "
                "VALUES(?,?,?,?,?)");
            if (!st.ok()) return false;
            st.bindString(0, actorType);
            st.bindString(1, actor);
            st.bindString(2, event);
            st.bindString(3, ip);
            st.bindString(4, detail);
            return st.execute();
        }

        bool purgeOldThrottles(MYSQL *conn)
        {
            return mysql_query(conn,
                "DELETE FROM auth_throttles WHERE updated_at < DATE_SUB(NOW(3), INTERVAL 7 DAY) "
                "AND (locked_until IS NULL OR locked_until < NOW(3))") == 0;
        }
    };
}

#endif
