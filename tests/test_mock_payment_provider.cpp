#include <cassert>

#include "MockPaymentProvider.hpp"

int main()
{
    using namespace hyperticket;

    PaymentProviderRequest payment;
    payment.paymentNo = "PY2026091600000001";
    payment.idempotencyKey = "idem-test-0001";
    payment.amountMinor = 12800;
    payment.currency = "CNY";

    MockPaymentProvider successProvider(100);
    PaymentProviderResult created = successProvider.createPayment(payment);
    assert(created.accepted);
    assert(created.state == ProviderPaymentState::Processing);
    assert(created.providerTransactionId == "MOCK-" + payment.paymentNo);

    payment.providerTransactionId = created.providerTransactionId;
    PaymentProviderResult settled = successProvider.queryPayment(payment);
    assert(settled.accepted);
    assert(settled.state == ProviderPaymentState::Succeeded);

    PaymentProviderResult closed = successProvider.closePayment(payment);
    assert(closed.accepted);
    assert(closed.state == ProviderPaymentState::Closed);

    RefundProviderRequest refund;
    refund.refundNo = "RF10001";
    refund.paymentNo = payment.paymentNo;
    refund.providerTransactionId = payment.providerTransactionId;
    refund.amountMinor = payment.amountMinor;
    refund.currency = payment.currency;
    PaymentProviderResult refunded = successProvider.refundPayment(refund);
    assert(refunded.accepted);
    assert(refunded.state == ProviderPaymentState::Succeeded);
    assert(!successProvider.verifyWebhook("{}", "invalid"));

    MockPaymentProvider failureProvider(0);
    PaymentProviderResult failed = failureProvider.queryPayment(payment);
    assert(failed.accepted);
    assert(failed.state == ProviderPaymentState::Failed);

    payment.amountMinor = 0;
    assert(!successProvider.createPayment(payment).accepted);
    refund.amountMinor = 0;
    assert(!successProvider.refundPayment(refund).accepted);
    return 0;
}
