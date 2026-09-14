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
        // 新建预定：v2 改为 PENDING（待支付），15 分钟内未支付由定时任务回收。
        bool insert(MYSQL *conn, int64_t userId, int64_t ticketId, int quantity,
                    const std::string &requestId = "")
        {
            MysqlStmt st(conn,
                "INSERT INTO reservations (user_id, ticket_id, quantity, status, expire_at, request_id) "
                "VALUES(?,?,?,'PENDING', DATE_ADD(NOW(), INTERVAL 15 MINUTE), NULLIF(?,''))");
            if (!st.ok()) return false;
            st.bindInt(0, userId);
            st.bindInt(1, ticketId);
            st.bindInt(2, quantity);
            st.bindString(3, requestId);
            return st.execute();
        }

        bool findByRequestId(MYSQL *conn, const std::string &requestId,
                             int64_t userId, Reservation &out)
        {
            MysqlStmt st(conn,
                "SELECT id, ticket_id, quantity, status, COALESCE(order_no,'') "
                "FROM reservations WHERE request_id = ? AND user_id = ?");
            if (!st.ok()) return false;
            st.bindString(0, requestId); st.bindInt(1, userId);
            if (!st.execute() || !st.bindResults(5) || !st.fetch()) return false;
            out.id=st.getInt(0); out.ticketId=st.getInt(1);
            out.quantity=static_cast<int>(st.getInt(2)); out.status=st.getString(3);
            out.orderNo=st.getString(4); out.userId=userId; return true;
        }

        // 支付：PENDING → CONFIRMED（仅本人、未过期）。返回是否真的改到了行。
        bool pay(MYSQL *conn, int64_t reservationId, int64_t userId)
        {
            MysqlStmt st(conn,
                "UPDATE reservations SET status = 'CONFIRMED', expire_at = NULL, updated_at = NOW() "
                "WHERE id = ? AND user_id = ? AND status = 'PENDING' AND expire_at > NOW()");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindInt(1, userId);
            return st.execute() && mysql_affected_rows(conn) > 0;
        }

        // 超时未支付的 PENDING 订单：锁定读取（定时回收任务在事务内调用）。
        std::vector<Reservation> lockExpiredPending(MYSQL *conn, int limit)
        {
            std::vector<Reservation> out;
            MysqlStmt st(conn,
                "SELECT id, ticket_id, quantity FROM reservations "
                "WHERE status = 'PENDING' AND expire_at <= NOW() "
                "ORDER BY expire_at LIMIT ? FOR UPDATE");
            if (!st.ok()) return out;
            st.bindInt(0, limit);
            if (!st.execute() || !st.bindResults(3)) return out;
            while (st.fetch())
            {
                Reservation r;
                r.id = st.getInt(0);
                r.ticketId = st.getInt(1);
                r.quantity = static_cast<int>(st.getInt(2));
                out.push_back(r);
            }
            return out;
        }

        // 标记为 EXPIRED（定时回收）。
        bool setExpired(MYSQL *conn, int64_t reservationId)
        {
            MysqlStmt st(conn,
                "UPDATE reservations SET status = 'EXPIRED', updated_at = NOW() WHERE id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
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

        // 读取订单当前状态（仅本人，无锁；PAY_QUERY 轮询用）。
        bool getOwnedStatus(MYSQL *conn, int64_t reservationId, int64_t userId, std::string &statusOut)
        {
            MysqlStmt st(conn,
                "SELECT status FROM reservations WHERE id = ? AND user_id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindInt(1, userId);
            if (!st.execute() || !st.bindResults(1)) return false;
            if (!st.fetch()) return false;
            statusOut = st.getString(0);
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

        // 查询某用户的全部预定（包括已取消），联表带票务信息、座位信息。
        std::vector<Reservation> listByUserTel(MYSQL *conn, const std::string &tel)
        {
            std::vector<Reservation> out;
            MysqlStmt st(conn,
                "SELECT r.id, COALESCE(r.order_no,''), r.ticket_id, t.title, t.venue, "
                "       t.event_date, r.status, r.quantity, "
                "       DATE_FORMAT(r.created_at,'%Y-%m-%d %H:%i:%s'), t.category, "
                "       COALESCE(s.seat_label,''), COALESCE(s.tier,''), COALESCE(s.price,0), "
                "       COALESCE(DATE_FORMAT(r.expire_at,'%Y-%m-%d %H:%i:%s'),''), t.price "
                "FROM reservations r "
                "JOIN users u ON r.user_id = u.id "
                "JOIN tickets t ON r.ticket_id = t.id "
                "LEFT JOIN seats s ON s.reservation_id = r.id "
                "WHERE u.tel = ? ORDER BY r.id DESC");
            if (!st.ok()) return out;
            st.bindString(0, tel);
            if (!st.execute() || !st.bindResults(15)) return out;
            while (st.fetch())
            {
                Reservation r;
                r.id = st.getInt(0);
                r.orderNo = st.getString(1);
                r.ticketId = st.getInt(2);
                r.ticketTitle = st.getString(3);
                r.ticketVenue = st.getString(4);
                r.eventDate = st.getString(5);
                r.status = st.getString(6);
                r.quantity = static_cast<int>(st.getInt(7));
                r.createdAt = st.getString(8);
                r.category = st.getString(9);
                r.seatLabel = st.getString(10);
                r.seatTier = st.getString(11);
                r.seatPrice = static_cast<int>(st.getInt(12));
                r.expireAt = st.getString(13);
                r.ticketPrice = static_cast<int>(st.getInt(14));
                out.push_back(r);
            }
            return out;
        }

        // 下单后生成并写入真实订单号。
        bool setOrderNo(MYSQL *conn, int64_t reservationId)
        {
            MysqlStmt st(conn,
                "UPDATE reservations "
                "SET order_no = CONCAT('HT', DATE_FORMAT(created_at,'%Y%m%d'), LPAD(id,6,'0')) "
                "WHERE id = ? AND order_no IS NULL");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            return st.execute();
        }

        // 根据订单号查询票务状态（扫码验证，无需认证）。
        bool findByOrderNo(MYSQL *conn, const std::string &orderNo, Reservation &out)
        {
            MysqlStmt st(conn,
                "SELECT r.id, r.order_no, r.ticket_id, t.title, t.venue, "
                "       t.event_date, r.status, COALESCE(s.seat_label,''), COALESCE(s.tier,'') "
                "FROM reservations r "
                "JOIN tickets t ON r.ticket_id = t.id "
                "LEFT JOIN seats s ON s.reservation_id = r.id "
                "WHERE r.order_no = ?");
            if (!st.ok()) return false;
            st.bindString(0, orderNo);
            if (!st.execute() || !st.bindResults(9)) return false;
            if (!st.fetch()) return false;
            out.id = st.getInt(0);
            out.orderNo = st.getString(1);
            out.ticketId = st.getInt(2);
            out.ticketTitle = st.getString(3);
            out.ticketVenue = st.getString(4);
            out.eventDate = st.getString(5);
            out.status = st.getString(6);
            out.seatLabel = st.getString(7);
            out.seatTier = st.getString(8);
            return true;
        }

        // 删除已取消或已过期的预定（仅限本人）。
        bool deleteOwned(MYSQL *conn, int64_t reservationId, int64_t userId)
        {
            MysqlStmt st(conn,
                "DELETE FROM reservations WHERE id = ? AND user_id = ? "
                "AND status IN ('CANCELLED','EXPIRED')");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindInt(1, userId);
            return st.execute() && mysql_affected_rows(conn) > 0;
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
