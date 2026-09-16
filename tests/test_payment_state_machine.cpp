#include <cassert>

#include "PaymentStateMachine.hpp"

int main()
{
    using hyperticket::canTransitionPayment;

    assert(canTransitionPayment("CREATED", "PROCESSING"));
    assert(canTransitionPayment("CREATED", "FAILED"));
    assert(canTransitionPayment("CREATED", "CLOSED"));
    assert(canTransitionPayment("PROCESSING", "SUCCEEDED"));
    assert(canTransitionPayment("PROCESSING", "FAILED"));
    assert(canTransitionPayment("PROCESSING", "CLOSED"));
    assert(canTransitionPayment("SUCCEEDED", "REFUNDING"));
    assert(canTransitionPayment("REFUNDING", "PARTIALLY_REFUNDED"));
    assert(canTransitionPayment("REFUNDING", "REFUNDED"));
    assert(canTransitionPayment("PARTIALLY_REFUNDED", "REFUNDING"));
    assert(canTransitionPayment("PARTIALLY_REFUNDED", "REFUNDED"));

    assert(!canTransitionPayment("CREATED", "SUCCEEDED"));
    assert(!canTransitionPayment("FAILED", "PROCESSING"));
    assert(!canTransitionPayment("SUCCEEDED", "REFUNDED"));
    assert(!canTransitionPayment("REFUNDED", "REFUNDING"));
    return 0;
}
