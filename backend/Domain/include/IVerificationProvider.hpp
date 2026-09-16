#ifndef HYPERTICKET_I_VERIFICATION_PROVIDER_HPP
#define HYPERTICKET_I_VERIFICATION_PROVIDER_HPP

#include <string>

namespace hyperticket
{
    class IVerificationProvider
    {
    public:
        virtual ~IVerificationProvider() = default;

        virtual bool sendCode(const std::string &channel,
                              const std::string &destination,
                              const std::string &code,
                              const std::string &purpose,
                              std::string &errorOut) = 0;

        virtual bool verifyCode(const std::string &code,
                                const std::string &storedHash) const = 0;
    };
}

#endif
