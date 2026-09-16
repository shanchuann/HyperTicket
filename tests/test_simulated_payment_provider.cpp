#include <cassert>
#include <stdexcept>

#include "SimulatedPaymentProvider.hpp"

int main()
{
    using namespace hyperticket;

    SimulatedPaymentProvider alipay("ALIPAY", 100, "test-webhook-secret");
    PaymentProviderRequest payment;
    payment.paymentNo = "PY2026091600000042";
    payment.idempotencyKey = "checkout-placeholder-42";
    payment.amountMinor = 19900;
    payment.currency = "CNY";

    const PaymentProviderResult created = alipay.createPayment(payment);
    assert(created.accepted);
    assert(created.state == ProviderPaymentState::Processing);
    assert(created.providerTransactionId == "SIM-ALIPAY-" + payment.paymentNo);

    payment.providerTransactionId = created.providerTransactionId;
    const PaymentProviderResult queried = alipay.queryPayment(payment);
    assert(queried.accepted);
    assert(queried.state == ProviderPaymentState::Succeeded);
    assert(alipay.closePayment(payment).state == ProviderPaymentState::Closed);

    RefundProviderRequest refund;
    refund.refundNo = "RF42";
    refund.paymentNo = payment.paymentNo;
    refund.providerTransactionId = payment.providerTransactionId;
    refund.amountMinor = payment.amountMinor;
    refund.currency = payment.currency;
    const PaymentProviderResult refunded = alipay.refundPayment(refund);
    assert(refunded.accepted);
    assert(refunded.providerTransactionId == "SIM-ALIPAY-REFUND-RF42");

    const std::string payload = "{\"event\":\"payment.succeeded\"}";
    const std::string signature = alipay.signWebhookForTest(payload);
    assert(signature.size() == 64);
    assert(alipay.verifyWebhook(payload, signature));
    assert(!alipay.verifyWebhook(payload + "x", signature));
    assert(!alipay.verifyWebhook(payload, "invalid"));

    SimulatedPaymentProvider wechat("WECHAT", 0, "");
    assert(wechat.name() == "WECHAT");
    assert(wechat.queryPayment(payment).state == ProviderPaymentState::Failed);
    assert(!wechat.verifyWebhook(payload, signature));

    bool rejected = false;
    try
    {
        SimulatedPaymentProvider invalid("UNKNOWN", 100, "secret");
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
