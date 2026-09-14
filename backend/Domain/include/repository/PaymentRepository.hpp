#ifndef HYPERTICKET_PAYMENT_REPOSITORY_HPP
#define HYPERTICKET_PAYMENT_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // payments 支付流水表的数据访问。
    // 状态机：PROCESSING →（定时结算）→ SUCCESS / FAILED；
    //         SUCCESS →（取消已支付订单）→ REFUNDED；
    //         PROCESSING →（结算成功但订单已失效的补偿）→ REFUNDED。
    // 带 FOR UPDATE 的方法须在 Service 开启的事务内调用。
    class PaymentRepository
    {
    public:
        // 发起支付：写入 PROCESSING 流水，金额按选座价/票面价·张数快照，
        // settle_at = 当前时间 + 模拟网关结算延迟。须在锁定 reservation 行的事务内调用。
        bool insertForReservation(MYSQL *conn, int64_t reservationId,
                                  const std::string &method, int settleDelayMs)
        {
            MysqlStmt st(conn,
                "INSERT INTO payments (reservation_id, user_id, amount, method, settle_at) "
                "SELECT r.id, r.user_id, COALESCE(s.price, t.price * r.quantity), ?, "
                "       DATE_ADD(NOW(3), INTERVAL ? MICROSECOND) "
                "FROM reservations r "
                "JOIN tickets t ON t.id = r.ticket_id "
                "LEFT JOIN seats s ON s.reservation_id = r.id "
                "WHERE r.id = ?");
            if (!st.ok()) return false;
            st.bindString(0, method);
            st.bindInt(1, static_cast<int64_t>(settleDelayMs) * 1000);
            st.bindInt(2, reservationId);
            return st.execute() && mysql_affected_rows(conn) > 0;
        }

        // 创建后生成支付单号（紧接 insert，用主键 id 保证唯一）。
        bool setPaymentNo(MYSQL *conn, int64_t paymentId)
        {
            MysqlStmt st(conn,
                "UPDATE payments "
                "SET payment_no = CONCAT('PY', DATE_FORMAT(created_at,'%Y%m%d'), LPAD(id,8,'0')) "
                "WHERE id = ? AND payment_no IS NULL");
            if (!st.ok()) return false;
            st.bindInt(0, paymentId);
            return st.execute();
        }

        // 查询订单当前进行中的支付流水（幂等：重复发起支付时复用返回）。
        bool findProcessingByResv(MYSQL *conn, int64_t reservationId, Payment &out)
        {
            MysqlStmt st(conn,
                "SELECT id, COALESCE(payment_no,''), amount, method, status FROM payments "
                "WHERE reservation_id = ? AND status = 'PROCESSING' ORDER BY id DESC LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            if (!st.execute() || !st.bindResults(5)) return false;
            if (!st.fetch()) return false;
            fill(st, out);
            out.reservationId = reservationId;
            return true;
        }

        // 查询订单最近一笔支付流水（仅限本人，PAY_QUERY 轮询用）。
        bool latestByResv(MYSQL *conn, int64_t reservationId, int64_t userId, Payment &out)
        {
            MysqlStmt st(conn,
                "SELECT id, COALESCE(payment_no,''), amount, method, status FROM payments "
                "WHERE reservation_id = ? AND user_id = ? ORDER BY id DESC LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindInt(1, userId);
            if (!st.execute() || !st.bindResults(5)) return false;
            if (!st.fetch()) return false;
            fill(st, out);
            out.reservationId = reservationId;
            out.userId = userId;
            return true;
        }

        // 到期未结算的 PROCESSING 流水：锁定读取（定时结算任务在事务内调用）。
        std::vector<Payment> lockDueProcessing(MYSQL *conn, int limit)
        {
            std::vector<Payment> out;
            MysqlStmt st(conn,
                "SELECT id, reservation_id, user_id FROM payments "
                "WHERE status = 'PROCESSING' AND settle_at <= NOW(3) "
                "ORDER BY settle_at LIMIT ? FOR UPDATE");
            if (!st.ok()) return out;
            st.bindInt(0, limit);
            if (!st.execute() || !st.bindResults(3)) return out;
            while (st.fetch())
            {
                Payment p;
                p.id = st.getInt(0);
                p.reservationId = st.getInt(1);
                p.userId = st.getInt(2);
                out.push_back(p);
            }
            return out;
        }

        // 结算：PROCESSING → SUCCESS/FAILED/REFUNDED（条件 UPDATE，幂等）。
        bool settleFromProcessing(MYSQL *conn, int64_t paymentId, const std::string &newStatus)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET status = ?, updated_at = NOW() "
                "WHERE id = ? AND status = 'PROCESSING'");
            if (!st.ok()) return false;
            st.bindString(0, newStatus);
            st.bindInt(1, paymentId);
            return st.execute() && mysql_affected_rows(conn) > 0;
        }

        // 退款：取消已支付订单时，把该订单的 SUCCESS 流水置为 REFUNDED。
        bool refundSuccessByResv(MYSQL *conn, int64_t reservationId)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET status = 'REFUNDED', updated_at = NOW() "
                "WHERE reservation_id = ? AND status = 'SUCCESS'");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            return st.execute() && mysql_affected_rows(conn) > 0;
        }

    private:
        static void fill(MysqlStmt &st, Payment &out)
        {
            out.id = st.getInt(0);
            out.paymentNo = st.getString(1);
            out.amount = static_cast<int>(st.getInt(2));
            out.method = st.getString(3);
            out.status = st.getString(4);
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_PAYMENT_REPOSITORY_HPP
