#ifndef HYPERTICKET_VERIFICATION_PROVIDER_HPP
#define HYPERTICKET_VERIFICATION_PROVIDER_HPP

#include "../../Common/include/AppConfig.hpp"
#include "../../Domain/include/IVerificationProvider.hpp"

namespace hyperticket
{
    class VerificationProvider : public IVerificationProvider
    {
    public:
        explicit VerificationProvider(const VerificationConfig &config) : config_(config) {}

        bool sendCode(const std::string &channel,
                      const std::string &destination,
                      const std::string &code,
                      const std::string &purpose,
                      std::string &errorOut) override;

        bool verifyCode(const std::string &code,
                        const std::string &storedHash) const override;

    private:
        bool writeDevelopmentInbox(const std::string &channel,
                                   const std::string &destination,
                                   const std::string &code,
                                   const std::string &purpose,
                                   std::string &errorOut) const;
        bool sendEmailCode(const std::string &destination,
                           const std::string &code,
                           const std::string &purpose,
                           std::string &errorOut) const;
        bool sendMockSmsCode(const std::string &destination,
                             std::string &errorOut) const;

        VerificationConfig config_;
    };
}

#endif
