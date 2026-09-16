#ifndef HYPERTICKET_I_PAYMENT_PROVIDER_HPP
#define HYPERTICKET_I_PAYMENT_PROVIDER_HPP

#include <cstdint>
#include <string>

namespace hyperticket
{
    enum class ProviderPaymentState { Processing, Succeeded, Failed, Closed };

    struct PaymentProviderRequest
    {
        std::string paymentNo;
        std::string providerTransactionId;
        std::string idempotencyKey;
        int64_t amountMinor = 0;
        std::string currency;
    };

    struct PaymentProviderResult
    {
        bool accepted = false;
        ProviderPaymentState state = ProviderPaymentState::Failed;
        std::string providerTransactionId;
        std::string reason;
    };

    struct RefundProviderRequest
    {
        std::string refundNo;
        std::string paymentNo;
        std::string providerTransactionId;
        int64_t amountMinor = 0;
        std::string currency;
    };

    class IPaymentProvider
    {
    public:
        virtual ~IPaymentProvider() = default;
        virtual std::string name() const = 0;
        virtual PaymentProviderResult createPayment(const PaymentProviderRequest &request) = 0;
        virtual PaymentProviderResult queryPayment(const PaymentProviderRequest &request) = 0;
        virtual PaymentProviderResult closePayment(const PaymentProviderRequest &request) = 0;
        virtual PaymentProviderResult refundPayment(const RefundProviderRequest &request) = 0;
        virtual bool verifyWebhook(const std::string &payload,
                                   const std::string &signature) const = 0;
    };
}

#endif
