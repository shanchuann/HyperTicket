#ifndef HYPERTICKET_VERIFICATION_SENDER_HPP
#define HYPERTICKET_VERIFICATION_SENDER_HPP

#include "../../Common/include/AppConfig.hpp"
#include "../../Domain/include/IVerificationSender.hpp"

namespace hyperticket
{
    class VerificationSender : public IVerificationSender
    {
    public:
        explicit VerificationSender(const VerificationConfig &config) : config_(config) {}

        bool sendEmailCode(const std::string &destination,
                           const std::string &code,
                           const std::string &purpose,
                           std::string &errorOut) override;
        bool sendMockSmsCode(const std::string &destination,
                             const std::string &code,
                             const std::string &purpose,
                             std::string &errorOut) override;

    private:
        VerificationConfig config_;
    };
}

#endif
