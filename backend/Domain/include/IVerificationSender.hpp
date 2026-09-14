#ifndef HYPERTICKET_I_VERIFICATION_SENDER_HPP
#define HYPERTICKET_I_VERIFICATION_SENDER_HPP

#include <string>

namespace hyperticket
{
    class IVerificationSender
    {
    public:
        virtual ~IVerificationSender() = default;
        virtual bool sendEmailCode(const std::string &destination,
                                   const std::string &code,
                                   const std::string &purpose,
                                   std::string &errorOut) = 0;
        virtual bool sendMockSmsCode(const std::string &destination,
                                     const std::string &code,
                                     const std::string &purpose,
                                     std::string &errorOut) = 0;
    };
}

#endif
