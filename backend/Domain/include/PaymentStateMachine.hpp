#ifndef HYPERTICKET_PAYMENT_STATE_MACHINE_HPP
#define HYPERTICKET_PAYMENT_STATE_MACHINE_HPP

#include <string>

namespace hyperticket
{
    inline bool canTransitionPayment(const std::string &from, const std::string &to)
    {
        if (from == "CREATED") return to == "PROCESSING" || to == "FAILED" || to == "CLOSED";
        if (from == "PROCESSING") return to == "SUCCEEDED" || to == "FAILED" || to == "CLOSED";
        if (from == "SUCCEEDED") return to == "REFUNDING";
        if (from == "REFUNDING") return to == "PARTIALLY_REFUNDED" || to == "REFUNDED";
        if (from == "PARTIALLY_REFUNDED") return to == "REFUNDING" || to == "REFUNDED";
        return false;
    }
}

#endif
