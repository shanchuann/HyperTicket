#include "../../include/service/TicketService.hpp"

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../include/service/Txn.hpp"
#include "../../../ChronoLite/include/Logger.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace hyperticket
{
    namespace
    {
        std::string newRequestId()
        {
            static const char hex[] = "0123456789abcdef";
            thread_local std::mt19937_64 rng{std::random_device{}()};
            std::string id = "ord_";
            for (int i = 0; i < 24; ++i) id.push_back(hex[rng() & 0xf]);
            return id;
        }
    }

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

        // 选座订单使用 seat_ids；保留单个 seat_id 兼容旧客户端。
        int64_t seatId = getIntField(req, "seat_id", 0);
        std::vector<int64_t> seatIds;
        const Json::Value &seatIdsValue = req["seat_ids"];
        if (!seatIdsValue.isNull())
        {
            if (!seatIdsValue.isArray() || seatIdsValue.empty() || seatId > 0)
                return makeError(err::kInvalidInput);
            std::unordered_set<int64_t> uniqueSeatIds;
            for (const Json::Value &value : seatIdsValue)
            {
                if (!value.isIntegral()) return makeError(err::kInvalidInput);
                const int64_t id = value.asInt64();
                if (id <= 0 || !uniqueSeatIds.insert(id).second)
                    return makeError(err::kInvalidInput);
                seatIds.push_back(id);
            }
            if (seatIds.size() > 6 || static_cast<int>(seatIds.size()) != qty)
                return makeError(err::kInvalidInput);
        }
        else if (seatId > 0)
        {
            qty = 1;
            seatIds.push_back(seatId);
        }

        if (!orderQueue_) return makeError("ORDER_QUEUE_UNAVAILABLE");

        std::string requestId = req.get("request_id", "").asString();
        if (requestId.empty()) requestId = newRequestId();
        if (requestId.size() < 8 || requestId.size() > 64 ||
            requestId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
            return makeError(err::kInvalidInput);

        Json::Value payload;
        payload["request_id"] = requestId;
        payload["user_id"] = static_cast<Json::Int64>(userId);
        payload["usertel"] = tel;
        payload["ticket_id"] = static_cast<Json::Int64>(tkId);
        payload["quantity"] = qty;
        payload["seat_ids"] = Json::arrayValue;
        for (const int64_t id : seatIds)
            payload["seat_ids"].append(static_cast<Json::Int64>(id));
        payload["created_ms"] = static_cast<Json::Int64>(nowMs());
        Json::StreamWriterBuilder wb; wb["indentation"] = "";
        std::string streamId;
        auto queued = orderQueue_->enqueue(tkId, qty, userId, requestId,
                                            Json::writeString(wb, payload), streamId);
        if (queued == IOrderQueue::EnqueueResult::Unavailable)
        {
            // Cold cache: initialize once from the DB. SET NX prevents two
            // concurrent cold requests from overwriting a deduction.
            MYSQL *conn=nullptr; shanchuan::ConnectionGuard guard(&conn,pool_);
            Ticket current;
            if (conn && ticketRepo_.findDetail(conn,tkId,current) &&
                orderQueue_->initializeStock(tkId,current.availableSeats))
                queued=orderQueue_->enqueue(tkId,qty,userId,requestId,
                                             Json::writeString(wb,payload),streamId);
        }
        if (queued == IOrderQueue::EnqueueResult::SoldOut) return makeError(err::kNoTicket);
        if (queued == IOrderQueue::EnqueueResult::Unavailable) return makeError("ORDER_QUEUE_UNAVAILABLE");

        Json::Value res = makeOk();
        res["request_id"] = requestId;
        res["order_status"] = queued == IOrderQueue::EnqueueResult::Duplicate ? "DUPLICATE" : "QUEUED";
        return res;
    }

    Json::Value TicketService::queryQueuedOrder(const Json::Value &req)
    {
        std::string tel; int64_t userId=0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);
        const std::string requestId=req.get("request_id","").asString();
        if (requestId.empty()) return makeError(err::kInvalidInput);
        IOrderQueue::Status s;
        if (orderQueue_ && orderQueue_->getStatus(requestId,s))
        {
            if (s.userId != userId) return makeError(err::kOrderNotFound);
            Json::Value res=makeOk(); res["request_id"]=requestId;
            res["order_status"]=s.state;
            if(!s.reservationId.empty()) res["reservation_id"]=s.reservationId;
            if(!s.reason.empty()) res["reason"]=s.reason;
            return res;
        }
        MYSQL *conn=nullptr; shanchuan::ConnectionGuard raii(&conn,pool_);
        Reservation existing;
        if (conn && resvRepo_.findByRequestId(conn,requestId,userId,existing))
        {
            Json::Value res=makeOk(); res["request_id"]=requestId;
            res["order_status"]=existing.status; res["reservation_id"]=std::to_string(existing.id);
            return res;
        }
        return makeError(err::kOrderNotFound);
    }

    int TicketService::processQueuedOrders(int maxMessages)
    {
        if (!orderQueue_) return 0;
        int processed=0;
        while (processed < maxMessages)
        {
            IOrderQueue::Message msg;
            if (!orderQueue_->consume(msg)) break;
            Json::Value p; Json::CharReaderBuilder rb; std::string errors;
            std::istringstream input(msg.payload);
            if (!Json::parseFromStream(rb,input,&p,&errors))
            {
                orderQueue_->terminalFailure(msg,0,0,0,"INVALID_MESSAGE");
                ++processed; continue;
            }
            const int64_t userId=getIntField(p,"user_id",0);
            const int64_t ticketId=getIntField(p,"ticket_id",0);
            const int qty=static_cast<int>(getIntField(p,"quantity",0));
            std::vector<int64_t> seatIds;
            const Json::Value &queuedSeatIds=p["seat_ids"];
            if(queuedSeatIds.isArray())
                for(const Json::Value &value:queuedSeatIds)
                    if(value.isIntegral()) seatIds.push_back(value.asInt64());
            const int64_t legacySeatId=getIntField(p,"seat_id",0);
            if(seatIds.empty() && legacySeatId>0) seatIds.push_back(legacySeatId);
            const int64_t createdMs=getIntField(p,"created_ms",nowMs());
            const std::string tel=p.get("usertel","").asString();
            std::sort(seatIds.begin(),seatIds.end());
            const bool duplicateSeats=std::adjacent_find(seatIds.begin(),seatIds.end())!=seatIds.end();
            if(userId<=0||ticketId<=0||qty<1||qty>6||duplicateSeats||
               (!seatIds.empty() && static_cast<int>(seatIds.size())!=qty))
            {
                orderQueue_->terminalFailure(msg,ticketId,qty,userId,"INVALID_MESSAGE");
                ++processed; continue;
            }
            MYSQL *conn=nullptr; shanchuan::ConnectionGuard guard(&conn,pool_);
            if(!conn)
            {
                if(orderQueue_->deliveryCount(msg.id)>=orderQueueMaxRetries_)
                    orderQueue_->terminalFailure(msg,ticketId,qty,userId,err::kDbUnavailable);
                break;
            }

            Reservation existing;
            if(resvRepo_.findByRequestId(conn,msg.requestId,userId,existing))
            {
                orderQueue_->setStatus(msg.requestId,existing.status,userId,std::to_string(existing.id));
                orderQueue_->acknowledge(msg.id); ++processed; continue;
            }

            Txn txn(conn);
            if(!txn.ok()) break;
            Ticket t; std::string failure;
            if(!ticketRepo_.lockForUpdate(conn,ticketId,t)) failure=err::kTicketNotFound;
            else if(t.status!=1) failure=err::kTicketOffline;
            else if(t.availableSeats<qty) failure=err::kNoTicket;
            else if(!ticketRepo_.adjustSeats(conn,ticketId,-qty)) failure=err::kDbUpdate;
            else if(!resvRepo_.insert(conn,userId,ticketId,qty,msg.requestId)) failure=err::kDbInsert;

            int64_t resvId=0;
            if(failure.empty())
            {
                resvId=static_cast<int64_t>(mysql_insert_id(conn));
                for(const int64_t selectedSeatId:seatIds)
                {
                    if(!seatRepo_.lockAndSell(conn,selectedSeatId,ticketId,resvId))
                    {
                        failure="SEAT_TAKEN";
                        break;
                    }
                }
                if(failure.empty())
                    resvRepo_.insertAuditLastInsert(conn,"CREATE","queue:"+msg.requestId+" user:"+tel);
            }
            if(!failure.empty() || !txn.commit())
            {
                txn.rollback();
                // An insert race may mean another consumer already committed it.
                if(resvRepo_.findByRequestId(conn,msg.requestId,userId,existing))
                {
                    orderQueue_->setStatus(msg.requestId,existing.status,userId,std::to_string(existing.id));
                    orderQueue_->acknowledge(msg.id); ++processed; continue;
                }
                // Logical failures are terminal. Infrastructure errors stay pending.
                if(failure==err::kTicketNotFound || failure==err::kTicketOffline ||
                   failure==err::kNoTicket || failure=="SEAT_TAKEN")
                {
                    orderQueue_->terminalFailure(msg,ticketId,qty,userId,failure);
                    if(failure==err::kTicketNotFound || failure==err::kTicketOffline || failure==err::kNoTicket)
                        stock_->set(ticketId,0); // stale-high cache: fail closed
                    ++processed; continue;
                }
                if(orderQueue_->deliveryCount(msg.id)>=orderQueueMaxRetries_)
                {
                    orderQueue_->terminalFailure(msg,ticketId,qty,userId,
                        failure.empty()?err::kDbUpdate:failure);
                    ++processed; continue;
                }
                break;
            }
            resvRepo_.setOrderNo(conn,resvId);
            stock_->invalidateTicketList();
            orderQueue_->setStatus(msg.requestId,"PENDING",userId,std::to_string(resvId));
            orderQueue_->recordConsumeDuration((nowMs()-createdMs)/1000.0);
            orderQueue_->acknowledge(msg.id); ++processed;
        }
        return processed;
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
        if (!seatRepo_.releaseByReservation(conn, r.id))
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
        resvRepo_.insertAudit(conn, r.id, "CANCEL", "user:" + tel);

        // 已支付订单取消只创建退款请求；退款结果由受信任 Provider 在定时任务中确认。
        if (r.status == "CONFIRMED")
        {
            Payment payment;
            if (payRepo_.lockSucceededByResv(conn, r.id, payment))
            {
                if (!payRepo_.beginFullRefund(conn, payment, "order cancelled by user"))
                {
                    txn.rollback();
                    return makeError(err::kDbUpdate);
                }
                resvRepo_.insertAudit(conn, r.id, "REFUND_REQUESTED", "user:" + tel);
            }
        }

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

        Txn txn(conn);
        if (!txn.ok()) return makeError(err::kDbBegin);
        if (!resvRepo_.deleteOwned(conn, index, userId))
        {
            txn.rollback();
            return makeError(err::kOrderNotFound);
        }
        if (!resvRepo_.insertAudit(conn, index, "HIDE", "user:" + tel) || !txn.commit())
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
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
                !ticketRepo_.adjustSeats(conn, r.ticketId, r.quantity) ||
                !seatRepo_.releaseByReservation(conn, r.id))
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
