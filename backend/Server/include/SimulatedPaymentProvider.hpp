#ifndef HYPERTICKET_SIMULATED_PAYMENT_PROVIDER_HPP
#define HYPERTICKET_SIMULATED_PAYMENT_PROVIDER_HPP

#include <string>

#include "../../Domain/include/IPaymentProvider.hpp"

namespace hyperticket
{
    // Development placeholder for channels that require merchant credentials.
    // This class never calls an external network and must not be used as proof
    // of payment in production.
    class SimulatedPaymentProvider : public IPaymentProvider
    {
    public:
        SimulatedPaymentProvider(std::string channel, int successRatePercent,
                                 std::string webhookSecret);

        std::string name() const override { return channel_; }
        PaymentProviderResult createPayment(const PaymentProviderRequest &request) override;
        PaymentProviderResult queryPayment(const PaymentProviderRequest &request) override;
        PaymentProviderResult closePayment(const PaymentProviderRequest &request) override;
        PaymentProviderResult refundPayment(const RefundProviderRequest &request) override;
        bool verifyWebhook(const std::string &payload,
                           const std::string &signature) const override;

        std::string signWebhookForTest(const std::string &payload) const;

    private:
        std::string transactionId(const std::string &paymentNo) const;

        std::string channel_;
        int successRatePercent_;
        std::string webhookSecret_;
    };
}

#endif
