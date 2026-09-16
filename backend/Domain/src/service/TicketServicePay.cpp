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

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard raii(&conn, pool_);
        if (!conn) return false;

        Txn txn(conn);
        if (!txn.ok()) return false;

        const std::vector<Payment> duePayments = payRepo_.lockDuePayments(conn, 200);
        int created = 0, succeeded = 0, failed = 0, closed = 0, refunded = 0;
        for (const Payment &candidate : duePayments)
        {
            // All order/payment paths lock in reservation -> payment order.
            Reservation lockedReservation;
            if (!resvRepo_.lockOwnedForUpdate(conn, candidate.reservationId,
                                              candidate.userId, lockedReservation))
                continue;
            Payment payment;
            if (!payRepo_.lockById(conn, candidate.id, payment) ||
                (payment.status != "CREATED" && payment.status != "PROCESSING"))
                continue;
            const auto providerIt = paymentProviders_.find(payment.provider);
            if (providerIt == paymentProviders_.end())
            {
                LOG_ERROR << "payment provider unavailable: " << payment.provider
                          << " payment=" << payment.paymentNo;
                continue;
            }
            IPaymentProvider *paymentProvider = providerIt->second;

            const std::string &orderStatus = lockedReservation.status;
            if (payment.status == "CREATED")
            {
                if (orderStatus != "PENDING")
                {
                    if (!payRepo_.transition(conn, payment.id, "CREATED", "CLOSED", "") ||
                        !payRepo_.appendEvent(conn, payment.id, "CREATED", "CLOSED", "SYSTEM", "order not payable"))
                    {
                        txn.rollback();
                        return false;
                    }
                    ++closed;
                    continue;
                }

                const PaymentProviderResult result = paymentProvider->createPayment(providerRequest(payment));
                const std::string next = result.accepted ? providerStateName(result.state) : "FAILED";
                if (!canTransitionPayment("CREATED", next) ||
                    !payRepo_.transition(conn, payment.id, "CREATED", next,
                                         result.providerTransactionId, paySettleDelayMs_) ||
                    !payRepo_.appendEvent(conn, payment.id, "CREATED", next, "PROVIDER", result.reason))
                {
                    txn.rollback();
                    return false;
                }
                ++created;
                if (next == "FAILED") ++failed;
                continue;
            }

            PaymentProviderResult result;
            if (orderStatus != "PENDING")
                result = paymentProvider->closePayment(providerRequest(payment));
            else
                result = paymentProvider->queryPayment(providerRequest(payment));

            std::string next = result.accepted ? providerStateName(result.state) : "FAILED";
            if (next == "PROCESSING")
            {
                if (!payRepo_.transition(conn, payment.id, "PROCESSING", "PROCESSING",
                                         result.providerTransactionId, paySettleDelayMs_))
                {
                    txn.rollback();
                    return false;
                }
                continue;
            }
            if (!canTransitionPayment("PROCESSING", next) ||
                !payRepo_.transition(conn, payment.id, "PROCESSING", next, result.providerTransactionId) ||
                !payRepo_.appendEvent(conn, payment.id, "PROCESSING", next, "PROVIDER", result.reason))
            {
                txn.rollback();
                return false;
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
        }

        const std::vector<Refund> dueRefunds = payRepo_.lockDueRefunds(conn, 200);
        for (const Refund &refund : dueRefunds)
        {
            const auto providerIt = paymentProviders_.find(refund.provider);
            if (providerIt == paymentProviders_.end())
            {
                LOG_ERROR << "refund provider unavailable: " << refund.provider
                          << " refund=" << refund.refundNo;
                continue;
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
            const bool updated = refundSucceeded
                ? payRepo_.completeRefund(conn, refund.id, refund.status,
                                          result.providerTransactionId)
                : payRepo_.retryRefund(conn, refund,
                    result.reason.empty() ? "provider_refund_failed" : result.reason,
                    retryDelaySeconds);
            if (!updated)
            {
                txn.rollback();
                return false;
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
        }

        if (!txn.commit())
        {
            txn.rollback();
            return false;
        }

        if (!duePayments.empty() || !dueRefunds.empty())
            LOG_INFO << "settleDuePayments: created=" << created << " succeeded=" << succeeded
                     << " failed=" << failed << " closed=" << closed << " refunded=" << refunded;
        return true;
    }
}
