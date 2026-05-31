#include "../../include/service/TicketService.hpp"

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/ServiceUtil.hpp"

namespace hyperticket
{
    // 简单的事务 RAII：构造 BEGIN，析构时若未提交则 ROLLBACK，避免遗漏回滚。
    namespace
    {
        class Txn
        {
        public:
            explicit Txn(MYSQL *conn) : conn_(conn)
            {
                began_ = (mysql_query(conn_, "BEGIN") == 0);
            }
            ~Txn()
            {
                if (began_ && !done_) mysql_query(conn_, "ROLLBACK");
            }
            bool ok() const { return began_; }
            bool commit()
            {
                if (mysql_query(conn_, "COMMIT") != 0) return false;
                done_ = true;
                return true;
            }
            void rollback()
            {
                mysql_query(conn_, "ROLLBACK");
                done_ = true;
            }

        private:
            MYSQL *conn_;
            bool began_ = false;
            bool done_ = false;
        };
    }

    Json::Value TicketService::orderTicket(const Json::Value &req)
    {
        // 身份来自 token，而非客户端自报 tel，杜绝越权下单。
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
        {
            return makeError(err::kUnauthorized);
        }
        int64_t tkId = req.get(field::kIndex, -1).asInt();
        if (tkId <= 0)
        {
            return makeError(err::kInvalidInput);
        }

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Txn txn(conn);
        if (!txn.ok()) return makeError(err::kDbBegin);

        Ticket t;
        if (!ticketRepo_.lockForUpdate(conn, tkId, t))
        {
            return makeError(err::kTicketNotFound);
        }
        if (t.status != 1)
        {
            return makeError(err::kTicketOffline);
        }
        if (t.availableSeats <= 0)
        {
            return makeError(err::kNoTicket);
        }
        if (!ticketRepo_.adjustSeats(conn, tkId, -1))
        {
            return makeError(err::kDbUpdate);
        }
        if (!resvRepo_.insert(conn, userId, tkId, 1))
        {
            return makeError(err::kDbInsert);
        }
        // 审计为尽力而为，紧接 insert 后用 LAST_INSERT_ID() 关联。
        resvRepo_.insertAuditLastInsert(conn, "CREATE", "user:" + tel);

        if (!txn.commit()) return makeError(err::kDbUpdate);
        return makeOk();
    }

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
            tmp["id"] = std::to_string(r.id);
            tmp["tk_id"] = std::to_string(r.ticketId);
            tmp["title"] = r.ticketTitle;
            tmp["addr"] = r.ticketVenue;
            tmp["use_date"] = r.eventDate;
            tmp["status"] = r.status;
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
        int64_t index = req.get(field::kIndex, -1).asInt();
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
            return makeError(err::kOrderNotFound);
        }
        if (r.status != "CONFIRMED")
        {
            return makeError(err::kOrderCannotCancel);
        }
        if (!resvRepo_.setCancelled(conn, r.id))
        {
            return makeError(err::kDbUpdate);
        }
        if (!ticketRepo_.adjustSeats(conn, r.ticketId, r.quantity))
        {
            return makeError(err::kDbUpdate);
        }
        resvRepo_.insertAudit(conn, r.id, "CANCEL", "user:" + tel);

        if (!txn.commit()) return makeError(err::kDbUpdate);
        return makeOk();
    }
} // namespace hyperticket
