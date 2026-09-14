#include "../../include/service/TicketService.hpp"

#include <random>

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../include/service/Txn.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

// v3 支付模块：把 v2 的"一条 UPDATE 即支付"升级为带流水的异步支付。
//
//   PAY_ORDER (20)  发起支付：校验订单 PENDING → 写 PROCESSING 流水（提交模拟网关）
//   定时结算任务     settle_at 到点后模拟网关回调：PROCESSING → SUCCESS/FAILED；
//                    结算成功但订单已失效（超时回收/取消）则补偿为 REFUNDED
//   PAY_QUERY (24)  前端轮询支付结果
//
// 并发关键点：发起支付/取消/超时回收/结算确认都先锁 reservation 行（FOR UPDATE
// 或条件 UPDATE），同一订单上的竞争操作被串行化；流水状态迁移全部是
// "WHERE status='PROCESSING'" 的条件 UPDATE，重复结算天然幂等。

namespace hyperticket
{
    namespace
    {
        bool isValidMethod(const std::string &m)
        {
            return m == "MOCK" || m == "ALIPAY" || m == "WECHAT";
        }
    }

    // ========== PAY_ORDER (type 20) — 发起支付 ==========
    Json::Value TicketService::payOrder(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        int64_t resvId = getIntField(req, field::kIndex, -1);
        if (resvId <= 0) return makeError(err::kInvalidInput);

        std::string method = req.get(field::kMethod, "MOCK").asString();
        if (!isValidMethod(method)) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Txn txn(conn);
        if (!txn.ok()) return makeError(err::kDbBegin);

        // 锁订单行：与取消/超时回收/结算串行化，同时保证下面的
        // "查进行中流水 → 不存在才新建" 在并发重复点击下仍然只建一笔。
        Reservation r;
        if (!resvRepo_.lockOwnedForUpdate(conn, resvId, userId, r))
        {
            txn.rollback();
            return makeError(err::kOrderNotFound);
        }
        if (r.status != "PENDING")
        {
            txn.rollback();
            return makeError("ORDER_NOT_PAYABLE");
        }

        // 幂等：该订单已有进行中的支付流水，直接复用返回
        Payment existing;
        if (payRepo_.findProcessingByResv(conn, resvId, existing))
        {
            txn.rollback(); // 只读路径
            Json::Value res = makeOk();
            res[field::kPaymentNo] = existing.paymentNo;
            res[field::kPaymentStatus] = existing.status;
            res[field::kAmount] = existing.amount;
            return res;
        }

        if (!payRepo_.insertForReservation(conn, resvId, method, paySettleDelayMs_))
        {
            txn.rollback();
            return makeError(err::kDbInsert);
        }
        int64_t payId = static_cast<int64_t>(mysql_insert_id(conn));
        payRepo_.setPaymentNo(conn, payId);
        resvRepo_.insertAudit(conn, resvId, "PAY_CREATE", "user:" + tel + " method:" + method);

        if (!txn.commit())
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }

        // 结算由定时任务在 settle_at 到点后完成，这里返回流水信息供前端轮询
        Payment created;
        if (!payRepo_.latestByResv(conn, resvId, userId, created))
            return makeError(err::kDbUnavailable);

        Json::Value res = makeOk();
        res[field::kPaymentNo] = created.paymentNo;
        res[field::kPaymentStatus] = created.status;
        res[field::kAmount] = created.amount;
        return res;
    }

    // ========== PAY_QUERY (type 24) — 轮询支付结果 ==========
    Json::Value TicketService::queryPayment(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        int64_t resvId = getIntField(req, field::kIndex, -1);
        if (resvId <= 0) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Payment p;
        if (!payRepo_.latestByResv(conn, resvId, userId, p))
            return makeError(err::kOrderNotFound); // 该订单没有支付流水

        // 一并带回订单状态，前端一次轮询即可同步刷新订单卡片
        std::string orderStatus;
        resvRepo_.getOwnedStatus(conn, resvId, userId, orderStatus);

        Json::Value res = makeOk();
        res[field::kPaymentNo] = p.paymentNo;
        res[field::kPaymentStatus] = p.status;
        res[field::kAmount] = p.amount;
        res[field::kMethod] = p.method;
        res["order_status"] = orderStatus;
        return res;
    }

    // ========== 定时任务：结算到期的 PROCESSING 支付流水 ==========
    // 模拟第三方网关的异步回调：事务内锁定到期流水 → 按成功率判定结果。
    // 成功时用条件 UPDATE 确认订单（PENDING+未过期才命中）；确认不到说明
    // 订单已被超时回收/取消——网关已扣款，补偿为 REFUNDED。
    bool TicketService::settleDuePayments()
    {
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return false;

        Txn txn(conn);
        if (!txn.ok()) return false;

        std::vector<Payment> due = payRepo_.lockDueProcessing(conn, 200);
        if (due.empty())
        {
            txn.rollback();
            return true;
        }

        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> roll(0, 99);

        int success = 0, failed = 0, refunded = 0;
        for (const Payment &p : due)
        {
            std::string outcome;
            if (roll(rng) < paySuccessRatePercent_)
            {
                if (resvRepo_.pay(conn, p.reservationId, p.userId))
                {
                    outcome = "SUCCESS";
                    ++success;
                }
                else
                {
                    outcome = "REFUNDED"; // 订单已失效，补偿退款
                    ++refunded;
                }
            }
            else
            {
                outcome = "FAILED"; // 订单保持 PENDING，可在截止前重新发起支付
                ++failed;
            }

            if (!payRepo_.settleFromProcessing(conn, p.id, outcome))
            {
                txn.rollback();
                return false;
            }
            resvRepo_.insertAudit(conn, p.reservationId, "PAY_" + outcome,
                                  outcome == "REFUNDED" ? "system:settle-compensate"
                                                        : "system:mock-gateway");
        }

        if (!txn.commit())
        {
            txn.rollback();
            return false;
        }

        LOG_INFO << "settleDuePayments: success=" << success
                 << " failed=" << failed << " refunded=" << refunded;
        return true;
    }
} // namespace hyperticket
