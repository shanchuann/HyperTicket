#ifndef HYPERTICKET_PAYMENT_REPOSITORY_HPP
#define HYPERTICKET_PAYMENT_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    class PaymentRepository
    {
    public:
        bool insertForReservation(MYSQL *conn, int64_t reservationId, int64_t userId,
                                  const std::string &provider, const std::string &idempotencyKey)
        {
            MysqlStmt st(conn,
                "INSERT INTO payments "
                "(reservation_id,user_id,amount_minor,currency,provider,client_idempotency_key,status,next_action_at) "
                "SELECT r.id,r.user_id,COALESCE(SUM(s.price),t.price*r.quantity)*100,'CNY',?,?,'CREATED',NOW(3) "
                "FROM reservations r JOIN tickets t ON t.id=r.ticket_id "
                "LEFT JOIN seats s ON s.reservation_id=r.id "
                "WHERE r.id=? AND r.user_id=? GROUP BY r.id,r.user_id,t.price,r.quantity");
            if (!st.ok()) return false;
            st.bindString(0, provider);
            st.bindString(1, idempotencyKey);
            st.bindInt(2, reservationId);
            st.bindInt(3, userId);
            return st.execute() && st.affectedRows() > 0;
        }

        bool setPaymentNo(MYSQL *conn, int64_t paymentId)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET payment_no=CONCAT('PY',DATE_FORMAT(created_at,'%Y%m%d'),LPAD(id,8,'0')) "
                "WHERE id=? AND payment_no IS NULL");
            if (!st.ok()) return false;
            st.bindInt(0, paymentId);
            return st.execute();
        }

        bool findByIdempotencyKey(MYSQL *conn, int64_t userId, const std::string &key, Payment &out)
        {
            MysqlStmt st(conn, selectColumns() +
                " FROM payments WHERE user_id=? AND client_idempotency_key=? LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, userId);
            st.bindString(1, key);
            return fetchOne(st, out);
        }

        bool findById(MYSQL *conn, int64_t paymentId, Payment &out)
        {
            MysqlStmt st(conn, selectColumns() + " FROM payments WHERE id=? LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, paymentId);
            return fetchOne(st, out);
        }

        bool latestByResv(MYSQL *conn, int64_t reservationId, int64_t userId, Payment &out)
        {
            MysqlStmt st(conn, selectColumns() +
                " FROM payments WHERE reservation_id=? AND user_id=? ORDER BY id DESC LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            st.bindInt(1, userId);
            return fetchOne(st, out);
        }

        bool findActiveByResv(MYSQL *conn, int64_t reservationId, Payment &out)
        {
            MysqlStmt st(conn, selectColumns() +
                " FROM payments WHERE reservation_id=? AND status IN ('CREATED','PROCESSING') "
                "ORDER BY id DESC LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            return fetchOne(st, out);
        }

        std::vector<Payment> listDuePayments(MYSQL *conn, int limit)
        {
            std::vector<Payment> out;
            MysqlStmt st(conn, selectColumns() +
                " FROM payments WHERE status IN ('CREATED','PROCESSING') AND next_action_at<=NOW(3) "
                "ORDER BY next_action_at,id LIMIT ?");
            if (!st.ok()) return out;
            st.bindInt(0, limit);
            if (!st.execute() || !st.bindResults(10)) return out;
            while (st.fetch())
            {
                Payment payment;
                fill(st, payment);
                out.push_back(payment);
            }
            return out;
        }

        bool claimPaymentAction(MYSQL *conn, int64_t paymentId,
                                const std::string &expected, int leaseMs,
                                std::string &actionToken)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET action_token=REPLACE(UUID(),'-',''),"
                "next_action_at=DATE_ADD(NOW(3),INTERVAL ? MICROSECOND),"
                "updated_at=NOW(3) WHERE id=? AND status=? AND next_action_at<=NOW(3)");
            if (!st.ok()) return false;
            st.bindInt(0, static_cast<int64_t>(leaseMs) * 1000);
            st.bindInt(1, paymentId);
            st.bindString(2, expected);
            if (!st.execute() || st.affectedRows() == 0) return false;
            MysqlStmt tokenQuery(conn, "SELECT action_token FROM payments WHERE id=? LIMIT 1");
            if (!tokenQuery.ok()) return false;
            tokenQuery.bindInt(0, paymentId);
            if (!tokenQuery.execute() || !tokenQuery.bindResults(1) || !tokenQuery.fetch()) return false;
            actionToken = tokenQuery.getString(0);
            return !actionToken.empty();
        }

        bool transitionClaimed(MYSQL *conn, int64_t paymentId,
                               const std::string &expected, const std::string &actionToken,
                               const std::string &next, const std::string &providerTransactionId,
                               int nextActionDelayMs = 0)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET status=?,provider_transaction_id=CASE WHEN ?='' THEN provider_transaction_id ELSE ? END,"
                "action_token='',next_action_at=DATE_ADD(NOW(3),INTERVAL ? MICROSECOND),updated_at=NOW(3) "
                "WHERE id=? AND status=? AND action_token=?");
            if (!st.ok()) return false;
            st.bindString(0, next);
            st.bindString(1, providerTransactionId);
            st.bindString(2, providerTransactionId);
            st.bindInt(3, static_cast<int64_t>(nextActionDelayMs) * 1000);
            st.bindInt(4, paymentId);
            st.bindString(5, expected);
            st.bindString(6, actionToken);
            return st.execute() && st.affectedRows() > 0;
        }

        bool retryClaimedPayment(MYSQL *conn, int64_t paymentId,
                                 const std::string &expected, const std::string &actionToken,
                                 int nextActionDelayMs)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET action_token='',"
                "next_action_at=DATE_ADD(NOW(3),INTERVAL ? MICROSECOND),updated_at=NOW(3) "
                "WHERE id=? AND status=? AND action_token=?");
            if (!st.ok()) return false;
            st.bindInt(0, static_cast<int64_t>(nextActionDelayMs) * 1000);
            st.bindInt(1, paymentId);
            st.bindString(2, expected);
            st.bindString(3, actionToken);
            return st.execute() && st.affectedRows() > 0;
        }

        bool lockById(MYSQL *conn, int64_t paymentId, Payment &out)
        {
            MysqlStmt st(conn, selectColumns() + " FROM payments WHERE id=? LIMIT 1 FOR UPDATE");
            if (!st.ok()) return false;
            st.bindInt(0, paymentId);
            return fetchOne(st, out);
        }

        bool transition(MYSQL *conn, int64_t paymentId, const std::string &expected,
                        const std::string &next, const std::string &providerTransactionId,
                        int nextActionDelayMs = 0)
        {
            MysqlStmt st(conn,
                "UPDATE payments SET status=?,provider_transaction_id=CASE WHEN ?='' THEN provider_transaction_id ELSE ? END,"
                "next_action_at=DATE_ADD(NOW(3),INTERVAL ? MICROSECOND),updated_at=NOW(3) "
                "WHERE id=? AND status=?");
            if (!st.ok()) return false;
            st.bindString(0, next);
            st.bindString(1, providerTransactionId);
            st.bindString(2, providerTransactionId);
            st.bindInt(3, static_cast<int64_t>(nextActionDelayMs) * 1000);
            st.bindInt(4, paymentId);
            st.bindString(5, expected);
            return st.execute() && st.affectedRows() > 0;
        }

        bool appendEvent(MYSQL *conn, int64_t paymentId, const std::string &from,
                         const std::string &to, const std::string &source,
                         const std::string &detail = "")
        {
            MysqlStmt st(conn,
                "INSERT INTO payment_events(payment_id,from_status,to_status,source,detail) VALUES(?,?,?,?,?)");
            if (!st.ok()) return false;
            st.bindInt(0, paymentId);
            st.bindString(1, from);
            st.bindString(2, to);
            st.bindString(3, source);
            st.bindString(4, detail);
            return st.execute();
        }

        bool lockSucceededByResv(MYSQL *conn, int64_t reservationId, Payment &out)
        {
            MysqlStmt st(conn, selectColumns() +
                " FROM payments WHERE reservation_id=? AND status='SUCCEEDED' "
                "ORDER BY id DESC LIMIT 1 FOR UPDATE");
            if (!st.ok()) return false;
            st.bindInt(0, reservationId);
            return fetchOne(st, out);
        }

        bool insertRefund(MYSQL *conn, const Payment &payment, const std::string &reason)
        {
            MysqlStmt st(conn,
                "INSERT INTO refunds(payment_id,refund_no,amount_minor,currency,status,reason,next_action_at) "
                "VALUES(?,CONCAT('RF',UUID_SHORT()),?,?,'CREATED',?,NOW(3))");
            if (!st.ok()) return false;
            st.bindInt(0, payment.id);
            st.bindInt(1, payment.amountMinor);
            st.bindString(2, payment.currency);
            st.bindString(3, reason);
            return st.execute();
        }

        std::vector<Refund> listDueRefunds(MYSQL *conn, int limit)
        {
            std::vector<Refund> out;
            MysqlStmt st(conn,
                "SELECT r.id,r.payment_id,r.refund_no,r.amount_minor,r.currency,r.status,"
                "p.payment_no,p.provider,COALESCE(p.provider_transaction_id,''),"
                "r.attempt_count,r.max_attempts "
                "FROM refunds r JOIN payments p ON p.id=r.payment_id "
                "WHERE r.status IN ('CREATED','PROCESSING') AND r.next_action_at<=NOW(3) "
                "ORDER BY r.next_action_at,r.id LIMIT ?");
            if (!st.ok()) return out;
            st.bindInt(0, limit);
            if (!st.execute() || !st.bindResults(11)) return out;
            while (st.fetch())
            {
                Refund refund;
                refund.id = st.getInt(0);
                refund.paymentId = st.getInt(1);
                refund.refundNo = st.getString(2);
                refund.amountMinor = st.getInt(3);
                refund.currency = st.getString(4);
                refund.status = st.getString(5);
                refund.paymentNo = st.getString(6);
                refund.provider = st.getString(7);
                refund.paymentProviderTransactionId = st.getString(8);
                refund.attemptCount = static_cast<int>(st.getInt(9));
                refund.maxAttempts = static_cast<int>(st.getInt(10));
                out.push_back(refund);
            }
            return out;
        }

        bool claimRefundAction(MYSQL *conn, int64_t refundId,
                               const std::string &expected, int leaseMs,
                               std::string &actionToken)
        {
            MysqlStmt st(conn,
                "UPDATE refunds SET action_token=REPLACE(UUID(),'-',''),"
                "next_action_at=DATE_ADD(NOW(3),INTERVAL ? MICROSECOND),"
                "updated_at=NOW(3) WHERE id=? AND status=? AND next_action_at<=NOW(3)");
            if (!st.ok()) return false;
            st.bindInt(0, static_cast<int64_t>(leaseMs) * 1000);
            st.bindInt(1, refundId);
            st.bindString(2, expected);
            if (!st.execute() || st.affectedRows() == 0) return false;
            MysqlStmt tokenQuery(conn, "SELECT action_token FROM refunds WHERE id=? LIMIT 1");
            if (!tokenQuery.ok()) return false;
            tokenQuery.bindInt(0, refundId);
            if (!tokenQuery.execute() || !tokenQuery.bindResults(1) || !tokenQuery.fetch()) return false;
            actionToken = tokenQuery.getString(0);
            return !actionToken.empty();
        }

        bool completeRefund(MYSQL *conn, int64_t refundId, const std::string &expected,
                            const std::string &actionToken, const std::string &providerRefundId)
        {
            MysqlStmt st(conn,
                "UPDATE refunds SET status='SUCCEEDED',provider_refund_id=?,failure_reason='',"
                "attempt_count=attempt_count+1,action_token='',next_action_at=NOW(3),updated_at=NOW(3) "
                "WHERE id=? AND status=? AND action_token=?");
            if (!st.ok()) return false;
            st.bindString(0, providerRefundId);
            st.bindInt(1, refundId);
            st.bindString(2, expected);
            st.bindString(3, actionToken);
            return st.execute() && st.affectedRows() > 0;
        }

        bool retryRefund(MYSQL *conn, const Refund &refund, const std::string &actionToken,
                         const std::string &failureReason,
                         int delaySeconds)
        {
            MysqlStmt st(conn,
                "UPDATE refunds SET status=IF(attempt_count+1>=max_attempts,'FAILED','PROCESSING'),"
                "failure_reason=?,attempt_count=attempt_count+1,action_token='',"
                "next_action_at=DATE_ADD(NOW(3),INTERVAL ? SECOND),updated_at=NOW(3) "
                "WHERE id=? AND status=? AND action_token=?");
            if (!st.ok()) return false;
            st.bindString(0, failureReason);
            st.bindInt(1, delaySeconds);
            st.bindInt(2, refund.id);
            st.bindString(3, refund.status);
            st.bindString(4, actionToken);
            return st.execute() && st.affectedRows() > 0;
        }

        bool beginFullRefund(MYSQL *conn, const Payment &payment, const std::string &reason)
        {
            if (!transition(conn, payment.id, "SUCCEEDED", "REFUNDING", "")) return false;
            if (!insertRefund(conn, payment, reason)) return false;
            return appendEvent(conn, payment.id, "SUCCEEDED", "REFUNDING", "SYSTEM", reason);
        }

    private:
        static std::string selectColumns()
        {
            return "SELECT id,COALESCE(payment_no,''),reservation_id,user_id,amount_minor,currency,provider,"
                   "COALESCE(provider_transaction_id,''),client_idempotency_key,status";
        }

        static bool fetchOne(MysqlStmt &st, Payment &out)
        {
            if (!st.execute() || !st.bindResults(10) || !st.fetch()) return false;
            fill(st, out);
            return true;
        }

        static void fill(MysqlStmt &st, Payment &out)
        {
            out.id = st.getInt(0);
            out.paymentNo = st.getString(1);
            out.reservationId = st.getInt(2);
            out.userId = st.getInt(3);
            out.amountMinor = st.getInt(4);
            out.currency = st.getString(5);
            out.provider = st.getString(6);
            out.providerTransactionId = st.getString(7);
            out.idempotencyKey = st.getString(8);
            out.status = st.getString(9);
        }
    };
}

#endif
