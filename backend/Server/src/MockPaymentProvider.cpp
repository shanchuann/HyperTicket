#include "../include/MockPaymentProvider.hpp"

#include <functional>

namespace hyperticket
{
    PaymentProviderResult MockPaymentProvider::createPayment(const PaymentProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = request.amountMinor > 0 && request.currency == "CNY";
        result.state = result.accepted ? ProviderPaymentState::Processing : ProviderPaymentState::Failed;
        result.providerTransactionId = "MOCK-" + request.paymentNo;
        if (!result.accepted) result.reason = "invalid_mock_payment_request";
        return result;
    }

    PaymentProviderResult MockPaymentProvider::queryPayment(const PaymentProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = true;
        result.providerTransactionId = request.providerTransactionId.empty()
            ? "MOCK-" + request.paymentNo : request.providerTransactionId;
        const size_t bucket = std::hash<std::string>{}(request.paymentNo) % 100;
        result.state = bucket < static_cast<size_t>(successRatePercent_)
            ? ProviderPaymentState::Succeeded : ProviderPaymentState::Failed;
        if (result.state == ProviderPaymentState::Failed) result.reason = "mock_payment_failed";
        return result;
    }

    PaymentProviderResult MockPaymentProvider::closePayment(const PaymentProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = true;
        result.state = ProviderPaymentState::Closed;
        result.providerTransactionId = request.providerTransactionId;
        return result;
    }

    PaymentProviderResult MockPaymentProvider::refundPayment(const RefundProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = request.amountMinor > 0;
        result.state = result.accepted ? ProviderPaymentState::Succeeded : ProviderPaymentState::Failed;
        result.providerTransactionId = "MOCK-REFUND-" + request.refundNo;
        if (!result.accepted) result.reason = "invalid_mock_refund_request";
        return result;
    }

    bool MockPaymentProvider::verifyWebhook(const std::string &,
                                            const std::string &) const
    {
        return false;
    }
}
