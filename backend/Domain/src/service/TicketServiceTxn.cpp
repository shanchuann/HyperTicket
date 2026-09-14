#include "../../include/service/TicketService.hpp"

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../include/service/Txn.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{

    Json::Value TicketService::orderTicket(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        int64_t tkId = getIntField(req, field::kIndex, -1);
        if (tkId <= 0) return makeError(err::kInvalidInput);

        // 购票张数（默认 1，上限 6 张/单，参考大麦限购规则）
        int qty = static_cast<int>(getIntField(req, field::kQuantity, 1));
        if (qty < 1 || qty > 6) return makeError(err::kInvalidInput);

        // 可选座位 id（选座模式下非零；选座单固定 1 张）
        int64_t seatId = getIntField(req, "seat_id", 0);
        if (seatId > 0) qty = 1;

        // ── Redis 预扣减：把"无票"请求挡在 MySQL 之前 ──
        // SoldOut 直接秒拒（不占 DB 连接、不产生行锁竞争）；
        // Ok 表示缓存扣减成功，继续走 DB 事务（MySQL 仍是真值，防超卖靠行锁）；
        // Unavailable（未命中/Redis 故障）降级直查 DB，之后用真值回填。
        auto decr = stock_->tryDecr(tkId, qty);
        if (decr == IStockCache::DecrResult::SoldOut)
            return makeError(err::kNoTicket);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn)
        {
            if (decr == IStockCache::DecrResult::Ok) stock_->incr(tkId, qty); // 补偿
            return makeError(err::kDbUnavailable);
        }

        // DB 事务失败时回补缓存的补偿动作（下单未成功且预扣过，则还回去）
        bool committed = false;
        auto compensate = [&]() {
            if (decr == IStockCache::DecrResult::Ok && !committed)
                stock_->incr(tkId, qty);
        };

        Txn txn(conn);
        if (!txn.ok()) { compensate(); return makeError(err::kDbBegin); }

        Ticket t;
        if (!ticketRepo_.lockForUpdate(conn, tkId, t)) { txn.rollback(); compensate(); return makeError(err::kTicketNotFound); }
        if (t.status != 1)
        {
            txn.rollback();
            stock_->set(tkId, 0); // 下架票：校正缓存，后续请求秒拒
            committed = true;     // 已用真值覆盖，无需 incr 补偿
            return makeError(err::kTicketOffline);
        }
        if (t.availableSeats < qty)
        {
            txn.rollback();
            stock_->set(tkId, t.availableSeats); // 用 DB 真值校正缓存
            committed = true;
            return makeError(err::kNoTicket);
        }

        if (!ticketRepo_.adjustSeats(conn, tkId, -qty)) { txn.rollback(); compensate(); return makeError(err::kDbUpdate); }
        if (!resvRepo_.insert(conn, userId, tkId, qty)) { txn.rollback(); compensate(); return makeError(err::kDbInsert); }

        // 必须在审计 INSERT 之前读取，否则 mysql_insert_id 返回的是审计行的 ID
        int64_t resvId = static_cast<int64_t>(mysql_insert_id(conn));

        resvRepo_.insertAuditLastInsert(conn, "CREATE", "user:" + tel);

        if (seatId > 0)
        {
            if (!seatRepo_.lockAndSell(conn, seatId, tkId, resvId))
            {
                txn.rollback();
                compensate();
                return makeError("SEAT_TAKEN");
            }
        }

        if (!txn.commit()) { txn.rollback(); compensate(); return makeError(err::kDbUpdate); }
        committed = true;

        // 缓存未命中时用 DB 真值回填（本次事务已扣 qty，真值 = availableSeats - qty）
        if (decr == IStockCache::DecrResult::Unavailable)
            stock_->set(tkId, t.availableSeats - qty);
        stock_->invalidateTicketList(); // 余票数变化，列表缓存失效

        // 事务提交后生成订单号（非事务操作，失败不影响下单结果）
        resvRepo_.setOrderNo(conn, resvId);

        // v2：订单为 PENDING 待支付，返回订单信息供前端跳转支付
        Json::Value res = makeOk();
        res["reservation_id"] = std::to_string(resvId);
        res["order_status"] = "PENDING";
        res["pay_deadline_minutes"] = 15;
        res["total_price"] = t.price * qty;
        return res;
    }

    // PAY_ORDER/PAY_QUERY/定时结算见 TicketServicePay.cpp（v3 支付模块）。

    Json::Value TicketService::viewMyTickets(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
        {
            return makeError(err::kUnauthorized);
        }
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Reservation> list = resvRepo_.listByUserTel(conn, tel);
        Json::Value res = makeOk();
        for (const Reservation &r : list)
        {
            Json::Value tmp;
            tmp["reservation_id"] = std::to_string(r.id);
            tmp["order_no"] = r.orderNo;
            tmp["tk_id"] = std::to_string(r.ticketId);
            tmp["title"] = r.ticketTitle;
            tmp["addr"] = r.ticketVenue;
            tmp["num"] = std::to_string(r.quantity);
            tmp["use_date"] = r.eventDate;
            tmp["status"] = r.status;
            tmp["created_at"] = r.createdAt;
            tmp["category"] = r.category;
            tmp["seat_label"] = r.seatLabel;
            tmp["seat_tier"] = r.seatTier;
            tmp["seat_price"] = r.seatPrice;
            tmp["expire_at"] = r.expireAt;       // PENDING 订单支付截止时间
            tmp["ticket_price"] = r.ticketPrice; // 票面单价
            res[field::kArr].append(tmp);
        }
        res[field::kNum] = static_cast<int>(list.size());
        return res;
    }

    Json::Value TicketService::cancelTicket(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
        {
            return makeError(err::kUnauthorized);
        }
        int64_t index = getIntField(req, field::kIndex, -1);
        if (index <= 0)
        {
            return makeError(err::kInvalidInput);
        }

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Txn txn(conn);
        if (!txn.ok()) return makeError(err::kDbBegin);

        Reservation r;
        if (!resvRepo_.lockOwnedForUpdate(conn, index, userId, r))
        {
            txn.rollback();
            return makeError(err::kOrderNotFound);
        }
        if (r.status != "CONFIRMED" && r.status != "PENDING")
        {
            txn.rollback();
            return makeError(err::kOrderCannotCancel);
        }
        if (!resvRepo_.setCancelled(conn, r.id))
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
        if (!ticketRepo_.adjustSeats(conn, r.ticketId, r.quantity))
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
        resvRepo_.insertAudit(conn, r.id, "CANCEL", "user:" + tel);

        // 已支付订单取消：同步把 SUCCESS 支付流水置为 REFUNDED（模拟退款）
        if (r.status == "CONFIRMED" && payRepo_.refundSuccessByResv(conn, r.id))
            resvRepo_.insertAudit(conn, r.id, "REFUND", "user:" + tel);

        if (!txn.commit())
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }

        // 取消成功：回补库存缓存并失效列表缓存
        stock_->incr(r.ticketId, r.quantity);
        stock_->invalidateTicketList();
        return makeOk();
    }

    // ========== DELETE_ORDER (type 16) ==========
    Json::Value TicketService::deleteOrder(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        int64_t index = getIntField(req, field::kIndex, -1);
        if (index <= 0) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        if (!resvRepo_.deleteOwned(conn, index, userId))
            return makeError(err::kOrderNotFound);

        return makeOk();
    }

    // ========== VIEW_SEATS (type 17) ==========
    Json::Value TicketService::viewSeats(const Json::Value &req)
    {
        int64_t tkId = getIntField(req, field::kIndex, -1);
        if (tkId <= 0) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        std::vector<Seat> seats = seatRepo_.listByTicket(conn, tkId);
        Json::Value res = makeOk();
        res["has_seats"] = !seats.empty();
        for (const Seat &s : seats)
        {
            Json::Value item;
            item["id"]     = static_cast<Json::Int64>(s.id);
            item["label"]  = s.seatLabel;
            item["row"]    = s.rowLabel;
            item["col"]    = s.colNum;
            item["tier"]   = s.tier;
            item["price"]  = s.price;
            item["status"] = s.status;
            res[field::kArr].append(item);
        }
        res[field::kNum] = static_cast<int>(seats.size());
        return res;
    }
    // ========== VERIFY_ORDER (type 18) — 公开接口，无需认证 ==========
    Json::Value TicketService::verifyOrder(const Json::Value &req)
    {
        std::string orderNo = req.get("order_no", "").asString();
        if (orderNo.size() < 4) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Reservation r;
        if (!resvRepo_.findByOrderNo(conn, orderNo, r))
            return makeError(err::kOrderNotFound);

        Json::Value res = makeOk();
        res["order_no"]     = r.orderNo;
        // 不能写 res["status"]——会覆盖 makeOk() 的协议字段 "OK"，前端会误判失败
        res["order_status"] = r.status;
        res["title"]        = r.ticketTitle;
        res["venue"]        = r.ticketVenue;
        res["event_date"]   = r.eventDate;
        res["seat_label"]   = r.seatLabel;
        res["seat_tier"]    = r.seatTier;
        return res;
    }

    // ========== 定时任务：回收超时未支付的 PENDING 订单 ==========
    // 事务内锁定超时订单 → 标记 EXPIRED → DB 回补库存；提交后回补 Redis 缓存。
    bool TicketService::expirePendingOrders()
    {
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return false;

        Txn txn(conn);
        if (!txn.ok()) return false;

        std::vector<Reservation> expired = resvRepo_.lockExpiredPending(conn, 100);
        if (expired.empty())
        {
            txn.rollback();
            return true;
        }

        for (const Reservation &r : expired)
        {
            if (!resvRepo_.setExpired(conn, r.id) ||
                !ticketRepo_.adjustSeats(conn, r.ticketId, r.quantity))
            {
                txn.rollback();
                return false;
            }
            resvRepo_.insertAudit(conn, r.id, "EXPIRE", "system:pay-timeout");
        }

        if (!txn.commit())
        {
            txn.rollback();
            return false;
        }

        // 提交成功后回补 Redis 库存并失效列表缓存
        for (const Reservation &r : expired)
            stock_->incr(r.ticketId, r.quantity);
        stock_->invalidateTicketList();
        LOG_INFO << "expirePendingOrders: reclaimed " << expired.size() << " orders";
        return true;
    }
} // namespace hyperticket
