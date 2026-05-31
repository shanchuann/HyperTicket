#ifndef HYPERTICKET_RESERVATION_REPOSITORY_HPP
#define HYPERTICKET_RESERVATION_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // reservations 表 + reservation_audit 审计表的数据访问。
    // 带 FOR UPDATE 的方法须在 Service 开启的事务内调用。
    class ReservationRepository
    {
    public:
        // 新建预定（CONFIRMED）。
        bool insert(MYSQL *conn, int64_t userId, int64_t ticketId, int quantity)
        {
            MysqlStmt st(conn,
                "INSERT INTO reservations (user_id, ticket_id, quantity, status) VALUES(?,?,?,'CONFIRMED')");
            if (!st.ok()) return false;
            st.bindInt(0, userId);
            st.bindInt(1, ticketId);
            st.bindInt(2, quantity);
            return st.execute();
        }

        // 按预定 id + 归属用户锁定并读取（FOR UPDATE，须在事务内）。未找到返回 false。
        bool lockOwnedForUpdate(MYSQL *conn, int64_t reservationId, int64_t userId, Reservation &out)
        {
            MysqlStmt st(conn,
                "SELECT r.id, r.ticket_id, r.quantity, r.status FROM reservations r "
                "WHERE r.id = ? AND r.user_id = ? FOR UPDATE");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindInt(1, userId);
            if (!st.execute() || !st.bindResults(4)) return false;
            if (!st.fetch()) return false;
            out.id = st.getInt(0);
            out.ticketId = st.getInt(1);
            out.quantity = static_cast<int>(st.getInt(2));
            out.status = st.getString(3);
            out.userId = userId;
            return true;
        }

        // 标记预定为已取消（软删除）。
        bool setCancelled(MYSQL *conn, int64_t reservationId)
        {
            MysqlStmt st(conn, "UPDATE reservations SET status = 'CANCELLED', updated_at = NOW() WHERE id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            return st.execute();
        }

        // 查询某用户的预定（联表带票务信息，排除已取消）。
        std::vector<Reservation> listByUserTel(MYSQL *conn, const std::string &tel)
        {
            std::vector<Reservation> out;
            MysqlStmt st(conn,
                "SELECT r.id, r.ticket_id, t.title, t.venue, t.event_date, r.status "
                "FROM reservations r JOIN users u ON r.user_id = u.id "
                "JOIN tickets t ON r.ticket_id = t.id "
                "WHERE u.tel = ? AND r.status != 'CANCELLED'");
            if (!st.ok()) return out;
            st.bindString(0, tel);
            if (!st.execute() || !st.bindResults(6)) return out;
            while (st.fetch())
            {
                Reservation r;
                r.id = st.getInt(0);
                r.ticketId = st.getInt(1);
                r.ticketTitle = st.getString(2);
                r.ticketVenue = st.getString(3);
                r.eventDate = st.getString(4);
                r.status = st.getString(5);
                out.push_back(r);
            }
            return out;
        }

        // 写审计流水。reservationId<0 表示用 LAST_INSERT_ID()（紧接 insert 后）。
        bool insertAuditLastInsert(MYSQL *conn, const std::string &action, const std::string &detail)
        {
            MysqlStmt st(conn,
                "INSERT INTO reservation_audit (reservation_id, action, detail) VALUES(LAST_INSERT_ID(), ?, ?)");
            if (!st.ok()) return false;
            st.bindString(0, action);
            st.bindString(1, detail);
            return st.execute();
        }

        bool insertAudit(MYSQL *conn, int64_t reservationId, const std::string &action, const std::string &detail)
        {
            MysqlStmt st(conn,
                "INSERT INTO reservation_audit (reservation_id, action, detail) VALUES(?, ?, ?)");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindString(1, action);
            st.bindString(2, detail);
            return st.execute();
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_RESERVATION_REPOSITORY_HPP
