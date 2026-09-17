#include "../../include/service/TicketService.hpp"

#include <cctype>
#include <exception>

#include "../../../Common/include/Protocol.hpp"
#include "../../../Common/include/Errors.hpp"
#include "../../include/PaymentStateMachine.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../include/service/Txn.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    namespace
    {
        bool isValidIdempotencyKey(const std::string &key)
        {
            if (key.size() < 8 || key.size() > 64) return false;
            for (unsigned char ch : key)
            {
                if (!std::isalnum(ch) && ch != '-' && ch != '_' && ch != '.' && ch != ':')
                    return false;
            }
            return true;
        }

        Json::Value paymentResponse(const Payment &payment)
        {
            Json::Value response = makeOk();
            response[field::kPaymentNo] = payment.paymentNo;
            response[field::kPaymentStatus] = payment.status;
            response[field::kAmountMinor] = static_cast<Json::Int64>(payment.amountMinor);
            response[field::kAmount] = static_cast<Json::Int64>(payment.amountMinor / 100);
            response[field::kCurrency] = payment.currency;
            response[field::kProvider] = payment.provider;
            response[field::kMethod] = payment.provider;
            response[field::kProviderTransactionId] = payment.providerTransactionId;
            response[field::kIdempotencyKey] = payment.idempotencyKey;
            return response;
        }

        PaymentProviderRequest providerRequest(const Payment &payment)
        {
            PaymentProviderRequest request;
            request.paymentNo = payment.paymentNo;
            request.providerTransactionId = payment.providerTransactionId;
            request.idempotencyKey = payment.idempotencyKey;
            request.amountMinor = payment.amountMinor;
            request.currency = payment.currency;
            return request;
        }

        std::string providerStateName(ProviderPaymentState state)
        {
            switch (state)
            {
            case ProviderPaymentState::Processing: return "PROCESSING";
            case ProviderPaymentState::Succeeded: return "SUCCEEDED";
            case ProviderPaymentState::Failed: return "FAILED";
            case ProviderPaymentState::Closed: return "CLOSED";
            }
            return "FAILED";
        }
    }

    Json::Value TicketService::payOrder(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        const int64_t reservationId = getIntField(req, field::kIndex, -1);
        const std::string provider = req.get(
            field::kProvider, req.get(field::kMethod, "MOCK")).asString();
        const std::string idempotencyKey = req.get(field::kIdempotencyKey, "").asString();
        if (reservationId <= 0 || !isValidIdempotencyKey(idempotencyKey))
            return makeError(err::kInvalidInput);
        if (provider != "MOCK" && provider != "ALIPAY" && provider != "WECHAT")
            return makeError(err::kInvalidInput);
        const auto providerIt = paymentProviders_.find(provider);
        if (providerIt == paymentProviders_.end())
            return makeError(err::kPaymentProviderUnavailable);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Txn txn(conn);
        if (!txn.ok()) return makeError(err::kDbBegin);

        Payment existing;
        if (payRepo_.findByIdempotencyKey(conn, userId, idempotencyKey, existing))
        {
            txn.rollback();
            if (existing.reservationId != reservationId || existing.provider != provider)
                return makeError(err::kPaymentIdempotencyConflict);
            return paymentResponse(existing);
        }

        Reservation reservation;
        if (!resvRepo_.lockOwnedForUpdate(conn, reservationId, userId, reservation))
        {
            txn.rollback();
            return makeError(err::kOrderNotFound);
        }
        if (reservation.status != "PENDING")
        {
            txn.rollback();
            return makeError("ORDER_NOT_PAYABLE");
        }

        Payment active;
        if (payRepo_.findActiveByResv(conn, reservationId, active))
        {
            txn.rollback();
            return makeError(err::kPaymentInProgress);
        }

        if (!payRepo_.insertForReservation(conn, reservationId, userId, provider, idempotencyKey))
        {
            txn.rollback();
            Payment raced;
            if (payRepo_.findByIdempotencyKey(conn, userId, idempotencyKey, raced))
            {
                if (raced.reservationId != reservationId || raced.provider != provider)
                    return makeError(err::kPaymentIdempotencyConflict);
                return paymentResponse(raced);
            }
            return makeError(err::kDbInsert);
        }

        const int64_t paymentId = static_cast<int64_t>(mysql_insert_id(conn));
        if (!payRepo_.setPaymentNo(conn, paymentId) ||
            !payRepo_.appendEvent(conn, paymentId, "", "CREATED", "CLIENT", "payment created") ||
            !resvRepo_.insertAudit(conn, reservationId, "PAY_CREATED", "user:" + tel + " provider:" + provider) ||
            !txn.commit())
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }

        Payment created;
        if (!payRepo_.findById(conn, paymentId, created)) return makeError(err::kDbUnavailable);
        return paymentResponse(created);
    }

    Json::Value TicketService::queryPayment(const Json::Value &req)
    {
        std::string tel;
        int64_t userId = 0;
        if (!sessions_->resolve(req.get(field::kToken, "").asString(), nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        const int64_t reservationId = getIntField(req, field::kIndex, -1);
        if (reservationId <= 0) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        Payment payment;
        if (!payRepo_.latestByResv(conn, reservationId, userId, payment))
            return makeError(err::kOrderNotFound);

        std::string orderStatus;
        resvRepo_.getOwnedStatus(conn, reservationId, userId, orderStatus);
        Json::Value response = paymentResponse(payment);
        response["order_status"] = orderStatus;
        return response;
    }

    bool TicketService::settleDuePayments()
    {
        if (paymentProviders_.empty()) return false;

        std::vector<Payment> duePayments;
        std::vector<Refund> dueRefunds;
        {
            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return false;
            duePayments = payRepo_.listDuePayments(conn, 200);
            dueRefunds = payRepo_.listDueRefunds(conn, 200);
        }

        constexpr int kProviderLeaseMs = 30000;
        int created = 0, succeeded = 0, failed = 0, closed = 0, refunded = 0;
        for (const Payment &candidate : duePayments)
        {
            const auto providerIt = paymentProviders_.find(candidate.provider);
            if (providerIt == paymentProviders_.end())
            {
                LOG_ERROR << "payment provider unavailable: " << candidate.provider
                          << " payment=" << candidate.paymentNo;
                continue;
            }

            Payment payment;
            std::string orderStatus;
            std::string actionToken;
            {
                MYSQL *conn = nullptr;
                shanchuan::ConnectionGuard raii(&conn, pool_);
                if (!conn) return false;
                Txn txn(conn);
                if (!txn.ok()) return false;

                Reservation reservation;
                if (!resvRepo_.lockOwnedForUpdate(conn, candidate.reservationId,
                                                  candidate.userId, reservation) ||
                    !payRepo_.lockById(conn, candidate.id, payment) ||
                    (payment.status != "CREATED" && payment.status != "PROCESSING"))
                {
                    txn.rollback();
                    continue;
                }
                orderStatus = reservation.status;

                if (payment.status == "CREATED" && orderStatus != "PENDING")
                {
                    if (!payRepo_.transition(conn, payment.id, "CREATED", "CLOSED", "") ||
                        !payRepo_.appendEvent(conn, payment.id, "CREATED", "CLOSED", "SYSTEM", "order not payable") ||
                        !txn.commit())
                    {
                        txn.rollback();
                        return false;
                    }
                    ++closed;
                    continue;
                }

                if (!payRepo_.claimPaymentAction(conn, payment.id, payment.status,
                                                 kProviderLeaseMs, actionToken) ||
                    !txn.commit())
                {
                    txn.rollback();
                    continue;
                }
            }

            PaymentProviderResult result;
            bool providerException = false;
            try
            {
                if (payment.status == "CREATED")
                    result = providerIt->second->createPayment(providerRequest(payment));
                else if (orderStatus != "PENDING")
                    result = providerIt->second->closePayment(providerRequest(payment));
                else
                    result = providerIt->second->queryPayment(providerRequest(payment));
            }
            catch (const std::exception &error)
            {
                providerException = true;
                result.reason = std::string("provider_exception:") + error.what();
            }
            catch (...)
            {
                providerException = true;
                result.reason = "provider_exception:unknown";
            }

            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return false;
            Txn txn(conn);
            if (!txn.ok()) return false;
            Reservation lockedReservation;
            Payment current;
            if (!resvRepo_.lockOwnedForUpdate(conn, payment.reservationId,
                                              payment.userId, lockedReservation) ||
                !payRepo_.lockById(conn, payment.id, current) ||
                current.status != payment.status)
            {
                txn.rollback();
                continue;
            }

            if (providerException)
            {
                if (!payRepo_.retryClaimedPayment(conn, payment.id, payment.status,
                                                  actionToken, paySettleDelayMs_) ||
                    !payRepo_.appendEvent(conn, payment.id, payment.status, payment.status,
                                          "PROVIDER_ERROR", result.reason) ||
                    !txn.commit())
                {
                    txn.rollback();
                    continue;
                }
                LOG_WARN << "payment provider call will retry: payment=" << payment.paymentNo
                         << " reason=" << result.reason;
                continue;
            }

            const std::string next = result.accepted ? providerStateName(result.state) : "FAILED";
            if (payment.status == "CREATED")
            {
                if (!canTransitionPayment("CREATED", next) ||
                    !payRepo_.transitionClaimed(conn, payment.id, "CREATED", actionToken,
                                                next, result.providerTransactionId,
                                                paySettleDelayMs_) ||
                    !payRepo_.appendEvent(conn, payment.id, "CREATED", next, "PROVIDER", result.reason) ||
                    !txn.commit())
                {
                    txn.rollback();
                    continue;
                }
                ++created;
                if (next == "FAILED") ++failed;
                continue;
            }

            if (next == "PROCESSING")
            {
                if (!payRepo_.transitionClaimed(conn, payment.id, "PROCESSING", actionToken,
                                                "PROCESSING", result.providerTransactionId,
                                                paySettleDelayMs_) ||
                    !txn.commit())
                {
                    txn.rollback();
                    continue;
                }
                continue;
            }
            if (!canTransitionPayment("PROCESSING", next) ||
                !payRepo_.transitionClaimed(conn, payment.id, "PROCESSING", actionToken,
                                            next, result.providerTransactionId) ||
                !payRepo_.appendEvent(conn, payment.id, "PROCESSING", next, "PROVIDER", result.reason))
            {
                txn.rollback();
                continue;
            }

            if (next == "SUCCEEDED")
            {
                if (resvRepo_.pay(conn, payment.reservationId, payment.userId))
                {
                    ++succeeded;
                    resvRepo_.insertAudit(conn, payment.reservationId, "PAY_SUCCEEDED", "system:provider-query");
                }
                else
                {
                    Payment succeededPayment = payment;
                    succeededPayment.status = "SUCCEEDED";
                    succeededPayment.providerTransactionId = result.providerTransactionId;
                    if (!payRepo_.beginFullRefund(conn, succeededPayment, "order not payable after settlement"))
                    {
                        txn.rollback();
                        return false;
                    }
                    ++refunded;
                }
            }
            else if (next == "FAILED") ++failed;
            else if (next == "CLOSED") ++closed;
            if (!txn.commit())
            {
                txn.rollback();
                return false;
            }
        }

        for (const Refund &refund : dueRefunds)
        {
            const auto providerIt = paymentProviders_.find(refund.provider);
            if (providerIt == paymentProviders_.end())
            {
                LOG_ERROR << "refund provider unavailable: " << refund.provider
                          << " refund=" << refund.refundNo;
                continue;
            }
            std::string actionToken;
            {
                MYSQL *conn = nullptr;
                shanchuan::ConnectionGuard raii(&conn, pool_);
                if (!conn) return false;
                Txn txn(conn);
                if (!txn.ok()) return false;
                if (!payRepo_.claimRefundAction(conn, refund.id, refund.status,
                                                kProviderLeaseMs, actionToken) ||
                    !txn.commit())
                {
                    txn.rollback();
                    continue;
                }
            }

            RefundProviderRequest request;
            request.refundNo = refund.refundNo;
            request.paymentNo = refund.paymentNo;
            request.providerTransactionId = refund.paymentProviderTransactionId;
            request.amountMinor = refund.amountMinor;
            request.currency = refund.currency;
            PaymentProviderResult result;
            try
            {
                result = providerIt->second->refundPayment(request);
            }
            catch (const std::exception &error)
            {
                result.accepted = false;
                result.reason = std::string("provider_exception:") + error.what();
            }
            catch (...)
            {
                result.accepted = false;
                result.reason = "provider_exception:unknown";
            }
            const bool refundSucceeded = result.accepted &&
                result.state == ProviderPaymentState::Succeeded;
            const int retryDelaySeconds = 1 << (refund.attemptCount < 8 ? refund.attemptCount : 8);

            MYSQL *conn = nullptr;
            shanchuan::ConnectionGuard raii(&conn, pool_);
            if (!conn) return false;
            Txn txn(conn);
            if (!txn.ok()) return false;
            const bool updated = refundSucceeded
                ? payRepo_.completeRefund(conn, refund.id, refund.status, actionToken,
                                          result.providerTransactionId)
                : payRepo_.retryRefund(conn, refund, actionToken,
                    result.reason.empty() ? "provider_refund_failed" : result.reason,
                    retryDelaySeconds);
            if (!updated)
            {
                txn.rollback();
                continue;
            }
            if (refundSucceeded)
            {
                if (!payRepo_.transition(conn, refund.paymentId, "REFUNDING", "REFUNDED", "") ||
                    !payRepo_.appendEvent(conn, refund.paymentId, "REFUNDING", "REFUNDED", "PROVIDER", refund.refundNo))
                {
                    txn.rollback();
                    return false;
                }
                ++refunded;
            }
            if (!txn.commit())
            {
                txn.rollback();
                return false;
            }
        }

        if (!duePayments.empty() || !dueRefunds.empty())
            LOG_INFO << "settleDuePayments: created=" << created << " succeeded=" << succeeded
                     << " failed=" << failed << " closed=" << closed << " refunded=" << refunded;
        return true;
    }
}
