#ifndef HYPERTICKET_AUTH_CHALLENGE_REPOSITORY_HPP
#define HYPERTICKET_AUTH_CHALLENGE_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    class AuthChallengeRepository
    {
    public:
        bool canSend(MYSQL *conn, const std::string &purpose,
                     const std::string &channel, const std::string &destination,
                     int cooldownSeconds, int dailyLimit)
        {
            MysqlStmt st(conn,
                "SELECT "
                "SUM(created_at > DATE_SUB(NOW(3), INTERVAL ? SECOND)), "
                "SUM(created_at >= CURRENT_DATE()) "
                "FROM auth_challenges WHERE purpose=? AND channel=? AND destination=?");
            if (!st.ok()) return false;
            st.bindInt(0, cooldownSeconds);
            st.bindString(1, purpose);
            st.bindString(2, channel);
            st.bindString(3, destination);
            if (!st.execute() || !st.bindResults(2) || !st.fetch()) return false;
            return st.getInt(0) == 0 && st.getInt(1) < dailyLimit;
        }

        bool insert(MYSQL *conn, const std::string &id, int64_t userId,
                    const std::string &subject, const std::string &destination,
                    const std::string &purpose, const std::string &channel,
                    const std::string &codeHash, int ttlSeconds, int maxAttempts)
        {
            MysqlStmt st(conn,
                "INSERT INTO auth_challenges(id,user_id,subject,destination,purpose,channel,code_hash,"
                "expires_at,max_attempts) VALUES(?,?,?,?,?,?,?,DATE_ADD(NOW(3), INTERVAL ? SECOND),?)");
            if (!st.ok()) return false;
            st.bindString(0, id);
            st.bindInt(1, userId);
            st.bindString(2, subject);
            st.bindString(3, destination);
            st.bindString(4, purpose);
            st.bindString(5, channel);
            st.bindString(6, codeHash);
            st.bindInt(7, ttlSeconds);
            st.bindInt(8, maxAttempts);
            return st.execute();
        }

        bool lockActive(MYSQL *conn, const std::string &id,
                        const std::string &purpose, AuthChallenge &out)
        {
            MysqlStmt st(conn,
                "SELECT id,IFNULL(user_id,0),subject,destination,purpose,channel,code_hash,"
                "attempts,max_attempts FROM auth_challenges WHERE id=? AND purpose=? "
                "AND consumed_at IS NULL AND verified_at IS NULL AND expires_at>NOW(3) "
                "AND attempts<max_attempts FOR UPDATE");
            if (!st.ok()) return false;
            st.bindString(0, id);
            st.bindString(1, purpose);
            if (!st.execute() || !st.bindResults(9) || !st.fetch()) return false;
            out.id = st.getString(0);
            out.userId = st.getInt(1);
            out.subject = st.getString(2);
            out.destination = st.getString(3);
            out.purpose = st.getString(4);
            out.channel = st.getString(5);
            out.codeHash = st.getString(6);
            out.attempts = static_cast<int>(st.getInt(7));
            out.maxAttempts = static_cast<int>(st.getInt(8));
            return true;
        }

        bool recordFailedAttempt(MYSQL *conn, const std::string &id)
        {
            MysqlStmt st(conn, "UPDATE auth_challenges SET attempts=attempts+1 WHERE id=?");
            if (!st.ok()) return false;
            st.bindString(0, id);
            return st.execute();
        }

        bool markVerified(MYSQL *conn, const std::string &id,
                          const std::string &grantHash, int grantTtlSeconds)
        {
            MysqlStmt st(conn,
                "UPDATE auth_challenges SET verified_at=NOW(3),grant_hash=?,"
                "grant_expires_at=DATE_ADD(NOW(3), INTERVAL ? SECOND) "
                "WHERE id=? AND verified_at IS NULL AND consumed_at IS NULL");
            if (!st.ok()) return false;
            st.bindString(0, grantHash);
            st.bindInt(1, grantTtlSeconds);
            st.bindString(2, id);
            return st.execute() && mysql_affected_rows(conn) == 1;
        }

        bool lockGrant(MYSQL *conn, const std::string &id,
                       const std::string &purpose, AuthChallenge &out)
        {
            MysqlStmt st(conn,
                "SELECT id,IFNULL(user_id,0),subject,destination,purpose,channel,grant_hash "
                "FROM auth_challenges WHERE id=? AND purpose=? AND verified_at IS NOT NULL "
                "AND consumed_at IS NULL AND grant_expires_at>NOW(3) FOR UPDATE");
            if (!st.ok()) return false;
            st.bindString(0, id);
            st.bindString(1, purpose);
            if (!st.execute() || !st.bindResults(7) || !st.fetch()) return false;
            out.id = st.getString(0);
            out.userId = st.getInt(1);
            out.subject = st.getString(2);
            out.destination = st.getString(3);
            out.purpose = st.getString(4);
            out.channel = st.getString(5);
            out.grantHash = st.getString(6);
            return true;
        }

        bool consume(MYSQL *conn, const std::string &id)
        {
            MysqlStmt st(conn,
                "UPDATE auth_challenges SET consumed_at=NOW(3) WHERE id=? AND consumed_at IS NULL");
            if (!st.ok()) return false;
            st.bindString(0, id);
            return st.execute() && mysql_affected_rows(conn) == 1;
        }

        bool remove(MYSQL *conn, const std::string &id)
        {
            MysqlStmt st(conn, "DELETE FROM auth_challenges WHERE id=?");
            if (!st.ok()) return false;
            st.bindString(0, id);
            return st.execute();
        }

        bool purgeExpired(MYSQL *conn)
        {
            return mysql_query(conn,
                "DELETE FROM auth_challenges WHERE "
                "(consumed_at IS NOT NULL AND consumed_at < DATE_SUB(NOW(3), INTERVAL 1 DAY)) "
                "OR (expires_at < DATE_SUB(NOW(3), INTERVAL 1 DAY))") == 0;
        }
    };
}

#endif
