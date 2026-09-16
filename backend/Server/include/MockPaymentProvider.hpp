#ifndef HYPERTICKET_MOCK_PAYMENT_PROVIDER_HPP
#define HYPERTICKET_MOCK_PAYMENT_PROVIDER_HPP

#include "../../Domain/include/IPaymentProvider.hpp"

namespace hyperticket
{
    class MockPaymentProvider : public IPaymentProvider
    {
    public:
        explicit MockPaymentProvider(int successRatePercent)
            : successRatePercent_(successRatePercent < 0 ? 0 :
                                  (successRatePercent > 100 ? 100 : successRatePercent)) {}

        std::string name() const override { return "MOCK"; }
        PaymentProviderResult createPayment(const PaymentProviderRequest &request) override;
        PaymentProviderResult queryPayment(const PaymentProviderRequest &request) override;
        PaymentProviderResult closePayment(const PaymentProviderRequest &request) override;
        PaymentProviderResult refundPayment(const RefundProviderRequest &request) override;
        bool verifyWebhook(const std::string &payload,
                           const std::string &signature) const override;

    private:
        int successRatePercent_;
    };
}

#endif
