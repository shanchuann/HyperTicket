#include "../include/SimulatedPaymentProvider.hpp"

#include <cstdint>
#include <iomanip>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace hyperticket
{
    namespace
    {
        uint64_t stableBucket(const std::string &value)
        {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char ch : value)
            {
                hash ^= ch;
                hash *= 1099511628211ULL;
            }
            return hash % 100;
        }

        std::string hmacSha256(const std::string &secret, const std::string &payload)
        {
            unsigned char digest[EVP_MAX_MD_SIZE];
            unsigned int length = 0;
            HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
                 reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
                 digest, &length);
            std::ostringstream output;
            output << std::hex << std::setfill('0');
            for (unsigned int i = 0; i < length; ++i)
                output << std::setw(2) << static_cast<unsigned int>(digest[i]);
            return output.str();
        }
    }

    SimulatedPaymentProvider::SimulatedPaymentProvider(
        std::string channel, int successRatePercent, std::string webhookSecret)
        : channel_(std::move(channel)),
          successRatePercent_(successRatePercent < 0 ? 0 :
                              (successRatePercent > 100 ? 100 : successRatePercent)),
          webhookSecret_(std::move(webhookSecret))
    {
        if (channel_ != "ALIPAY" && channel_ != "WECHAT")
            throw std::invalid_argument("simulated channel must be ALIPAY or WECHAT");
    }

    std::string SimulatedPaymentProvider::transactionId(const std::string &paymentNo) const
    {
        return "SIM-" + channel_ + "-" + paymentNo;
    }

    PaymentProviderResult SimulatedPaymentProvider::createPayment(
        const PaymentProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = request.amountMinor > 0 && request.currency == "CNY" &&
                          !request.paymentNo.empty() && !request.idempotencyKey.empty();
        result.state = result.accepted ? ProviderPaymentState::Processing
                                       : ProviderPaymentState::Failed;
        if (result.accepted) result.providerTransactionId = transactionId(request.paymentNo);
        else result.reason = "invalid_simulated_" + channel_ + "_request";
        return result;
    }

    PaymentProviderResult SimulatedPaymentProvider::queryPayment(
        const PaymentProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = !request.paymentNo.empty();
        if (!result.accepted)
        {
            result.state = ProviderPaymentState::Failed;
            result.reason = "simulated_payment_not_found";
            return result;
        }
        result.providerTransactionId = request.providerTransactionId.empty()
            ? transactionId(request.paymentNo) : request.providerTransactionId;
        result.state = stableBucket(channel_ + ":" + request.paymentNo) <
                               static_cast<uint64_t>(successRatePercent_)
            ? ProviderPaymentState::Succeeded : ProviderPaymentState::Failed;
        if (result.state == ProviderPaymentState::Failed)
            result.reason = "simulated_" + channel_ + "_payment_failed";
        return result;
    }

    PaymentProviderResult SimulatedPaymentProvider::closePayment(
        const PaymentProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = !request.paymentNo.empty();
        result.state = result.accepted ? ProviderPaymentState::Closed
                                       : ProviderPaymentState::Failed;
        result.providerTransactionId = request.providerTransactionId;
        if (!result.accepted) result.reason = "simulated_payment_not_found";
        return result;
    }

    PaymentProviderResult SimulatedPaymentProvider::refundPayment(
        const RefundProviderRequest &request)
    {
        PaymentProviderResult result;
        result.accepted = request.amountMinor > 0 && request.currency == "CNY" &&
                          !request.refundNo.empty() && !request.paymentNo.empty();
        result.state = result.accepted ? ProviderPaymentState::Succeeded
                                       : ProviderPaymentState::Failed;
        if (result.accepted)
            result.providerTransactionId = "SIM-" + channel_ + "-REFUND-" + request.refundNo;
        else result.reason = "invalid_simulated_refund_request";
        return result;
    }

    std::string SimulatedPaymentProvider::signWebhookForTest(const std::string &payload) const
    {
        if (webhookSecret_.empty()) return "";
        return hmacSha256(webhookSecret_, payload);
    }

    bool SimulatedPaymentProvider::verifyWebhook(const std::string &payload,
                                                  const std::string &signature) const
    {
        if (webhookSecret_.empty() || signature.size() != 64) return false;
        const std::string expected = signWebhookForTest(payload);
        return expected.size() == signature.size() &&
               CRYPTO_memcmp(expected.data(), signature.data(), expected.size()) == 0;
    }
}
