#include "../../include/service/TicketService.hpp"

#include "../../../Common/include/Errors.hpp"
#include "../../../Common/include/Protocol.hpp"
#include "../../include/ServiceUtil.hpp"
#include "../../include/service/Txn.hpp"
#include "../../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    namespace
    {
        bool validEmail(const std::string &email)
        {
            const size_t at = email.find('@');
            return !email.empty() && email.size() <= 254 &&
                   at > 0 && at + 1 < email.size() &&
                   email.find('@', at + 1) == std::string::npos &&
                   email.find('\r') == std::string::npos && email.find('\n') == std::string::npos;
        }

        bool splitGrant(const std::string &token, std::string &id, std::string &secret)
        {
            const size_t dot = token.find('.');
            if (dot == std::string::npos) return false;
            id = token.substr(0, dot);
            secret = token.substr(dot + 1);
            return id.size() == 64 && secret.size() == 64;
        }

        Json::Value challengeResponse(const std::string &id, int ttlSeconds)
        {
            Json::Value response = makeOk();
            response[field::kChallengeId] = id;
            response["expires_in_seconds"] = ttlSeconds;
            return response;
        }
    }

    Json::Value TicketService::requestVerification(const Json::Value &req)
    {
        if (!verificationSender_) return makeError("VERIFICATION_UNAVAILABLE");
        const std::string tel = req.get(field::kUserTel, "").asString();
        const std::string channel = req.get(field::kChannel, "EMAIL").asString();
        const std::string destination = channel == "EMAIL"
            ? req.get(field::kEmail, "").asString() : tel;
        if (tel.size() != 11 || (channel != "EMAIL" && channel != "SMS") ||
            (channel == "EMAIL" && !validEmail(destination)) ||
            (channel == "SMS" && !mockSmsEnabled_))
            return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);

        const std::string ip = req.get("_client_ip", "").asString();
        const std::string device = req.get("client_id", "").asString().substr(0, 128);
        int64_t retryAfter = 0;
        if ((!ip.empty() && (!authRepo_.isBlocked(conn, "verification_ip", ip, retryAfter) || retryAfter > 0)) ||
            (!device.empty() && (!authRepo_.isBlocked(conn, "verification_device", device, retryAfter) || retryAfter > 0)) ||
            !authRepo_.isBlocked(conn, "verification_destination", destination, retryAfter) || retryAfter > 0)
            return makeError("VERIFICATION_RATE_LIMITED");

        User existing;
        if (userRepo_.findByTel(conn, tel, existing) ||
            (channel == "EMAIL" && userRepo_.findByTelOrEmail(conn, destination, existing)))
            return makeError("ACCOUNT_ALREADY_EXISTS");
        if (!challengeRepo_.canSend(conn, "REGISTER", channel, destination,
                                    resendCooldownSeconds_))
            return makeError("VERIFICATION_COOLDOWN");

        const std::string id = secureRandomHex();
        const std::string code = generateNumericCode();
        if (!challengeRepo_.insert(conn, id, 0, tel, destination, "REGISTER", channel,
                                   hashPassword(code), codeTtlSeconds_, verificationMaxAttempts_))
            return makeError(err::kDbInsert);

        std::string deliveryError;
        const bool delivered = channel == "EMAIL"
            ? verificationSender_->sendEmailCode(destination, code, "REGISTER", deliveryError)
            : verificationSender_->sendMockSmsCode(destination, code, "REGISTER", deliveryError);
        if (!delivered)
        {
            challengeRepo_.remove(conn, id);
            authRepo_.audit(conn, "anonymous", tel, "VERIFICATION_DELIVERY_FAILED",
                            req.get("_client_ip", "").asString(), deliveryError);
            return makeError("VERIFICATION_DELIVERY_FAILED");
        }

        authRepo_.audit(conn, "anonymous", tel, "VERIFICATION_SENT",
                        req.get("_client_ip", "").asString(), channel);
        if (!ip.empty()) authRepo_.recordFailure(conn, "verification_ip", ip, 900, 20, 900);
        if (!device.empty()) authRepo_.recordFailure(conn, "verification_device", device, 900, 10, 900);
        authRepo_.recordFailure(conn, "verification_destination", destination, 86400, 10, 86400);
        Json::Value response = challengeResponse(id, codeTtlSeconds_);
        if (channel == "SMS" && exposeMockSmsCode_) response["mock_code"] = code;
        return response;
    }

    Json::Value TicketService::verifyRegistrationCode(const Json::Value &req)
    {
        const std::string id = req.get(field::kChallengeId, "").asString();
        const std::string code = req.get(field::kCode, "").asString();
        if (id.size() != 64 || code.size() != 6) return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        Txn txn(conn);
        AuthChallenge challenge;
        if (!txn.ok() || !challengeRepo_.lockActive(conn, id, "REGISTER", challenge))
        {
            txn.rollback();
            return makeError("INVALID_OR_EXPIRED_CODE");
        }
        bool rehash = false;
        if (!verifyPassword(code, challenge.codeHash, rehash))
        {
            challengeRepo_.recordFailedAttempt(conn, id);
            txn.commit();
            return makeError("INVALID_OR_EXPIRED_CODE");
        }
        const std::string secret = secureRandomHex();
        if (!challengeRepo_.markVerified(conn, id, hashPassword(secret), grantTtlSeconds_) || !txn.commit())
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
        Json::Value response = makeOk();
        response[field::kVerificationToken] = id + "." + secret;
        response["expires_in_seconds"] = grantTtlSeconds_;
        return response;
    }

    bool TicketService::consumeGrant(MYSQL *conn, const std::string &token,
                                     const std::string &purpose,
                                     AuthChallenge &challengeOut)
    {
        std::string id, secret;
        if (!splitGrant(token, id, secret) ||
            !challengeRepo_.lockGrant(conn, id, purpose, challengeOut))
            return false;
        bool rehash = false;
        return verifyPassword(secret, challengeOut.grantHash, rehash) &&
               challengeRepo_.consume(conn, id);
    }

    Json::Value TicketService::requestPasswordReset(const Json::Value &req)
    {
        const std::string account = req.get("account", "").asString();
        const std::string channel = req.get(field::kChannel, "EMAIL").asString();
        if (account.empty() || (channel != "EMAIL" && channel != "SMS"))
            return makeError(err::kInvalidInput);

        // Always return an indistinguishable response and challenge-shaped ID.
        std::string responseId = secureRandomHex();
        Json::Value response = challengeResponse(responseId, codeTtlSeconds_);
        response["message"] = "If the account exists, a verification code has been sent.";
        if (channel == "SMS" && exposeMockSmsCode_)
            response["mock_code"] = generateNumericCode();

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        const std::string ip = req.get("_client_ip", "").asString();
        const std::string device = req.get("client_id", "").asString().substr(0, 128);
        int64_t retryAfter = 0;
        if ((!ip.empty() && (!authRepo_.isBlocked(conn, "verification_ip", ip, retryAfter) || retryAfter > 0)) ||
            (!device.empty() && (!authRepo_.isBlocked(conn, "verification_device", device, retryAfter) || retryAfter > 0)))
            return response;
        User user;
        if (!userRepo_.findByTelOrEmail(conn, account, user) || user.status != 1 ||
            !verificationSender_ || (channel == "SMS" && !mockSmsEnabled_))
            return response;

        const std::string destination = channel == "EMAIL" ? user.email : user.tel;
        if ((channel == "EMAIL" && (!user.emailVerified || !validEmail(destination))) ||
            (channel == "SMS" && !user.phoneVerified) ||
            !authRepo_.isBlocked(conn, "verification_destination", destination, retryAfter) ||
            retryAfter > 0 ||
            !challengeRepo_.canSend(conn, "PASSWORD_RESET", channel, destination,
                                    resendCooldownSeconds_))
            return response;

        const std::string code = generateNumericCode();
        if (!challengeRepo_.insert(conn, responseId, user.id, user.tel, destination,
                                   "PASSWORD_RESET", channel, hashPassword(code),
                                   codeTtlSeconds_, verificationMaxAttempts_))
            return response;

        std::string deliveryError;
        const bool delivered = channel == "EMAIL"
            ? verificationSender_->sendEmailCode(destination, code, "PASSWORD_RESET", deliveryError)
            : verificationSender_->sendMockSmsCode(destination, code, "PASSWORD_RESET", deliveryError);
        if (!delivered)
        {
            challengeRepo_.remove(conn, responseId);
            LOG_ERROR << "Password reset delivery failed: " << deliveryError;
            return response;
        }
        authRepo_.audit(conn, "user", user.tel, "PASSWORD_RESET_REQUESTED",
                        req.get("_client_ip", "").asString(), channel);
        if (!ip.empty()) authRepo_.recordFailure(conn, "verification_ip", ip, 900, 20, 900);
        if (!device.empty()) authRepo_.recordFailure(conn, "verification_device", device, 900, 10, 900);
        authRepo_.recordFailure(conn, "verification_destination", destination, 86400, 10, 86400);
        if (channel == "SMS" && exposeMockSmsCode_) response["mock_code"] = code;
        return response;
    }

    Json::Value TicketService::verifyPasswordReset(const Json::Value &req)
    {
        const std::string id = req.get(field::kChallengeId, "").asString();
        const std::string code = req.get(field::kCode, "").asString();
        if (id.size() != 64 || code.size() != 6) return makeError(err::kInvalidInput);
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        Txn txn(conn);
        AuthChallenge challenge;
        if (!txn.ok() || !challengeRepo_.lockActive(conn, id, "PASSWORD_RESET", challenge))
        {
            txn.rollback();
            static const std::string dummy = hashPassword("000000");
            bool ignored = false;
            verifyPassword(code, dummy, ignored);
            return makeError("INVALID_OR_EXPIRED_CODE");
        }
        bool rehash = false;
        if (!verifyPassword(code, challenge.codeHash, rehash))
        {
            challengeRepo_.recordFailedAttempt(conn, id);
            txn.commit();
            return makeError("INVALID_OR_EXPIRED_CODE");
        }
        const std::string secret = secureRandomHex();
        if (!challengeRepo_.markVerified(conn, id, hashPassword(secret), grantTtlSeconds_) || !txn.commit())
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
        Json::Value response = makeOk();
        response[field::kResetToken] = id + "." + secret;
        response["expires_in_seconds"] = grantTtlSeconds_;
        return response;
    }

    Json::Value TicketService::confirmPasswordReset(const Json::Value &req)
    {
        const std::string token = req.get(field::kResetToken, "").asString();
        const std::string password = req.get("new_password", "").asString();
        if (!isStrongPassword(password)) return makeError(err::kWeakPassword);
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        Txn txn(conn);
        AuthChallenge challenge;
        if (!txn.ok() || !consumeGrant(conn, token, "PASSWORD_RESET", challenge))
        {
            txn.rollback();
            return makeError("INVALID_OR_EXPIRED_RESET_TOKEN");
        }
        if (!userRepo_.updatePasswordHashById(conn, challenge.userId, hashPassword(password)))
        {
            txn.rollback();
            return makeError(err::kDbUpdate);
        }
        authRepo_.audit(conn, "user", challenge.subject, "PASSWORD_RESET_COMPLETED",
                        req.get("_client_ip", "").asString(), "all_sessions_revoked");
        if (!txn.commit()) return makeError(err::kDbUpdate);
        sessions_->removeAllForUser(challenge.userId);
        return makeOk();
    }

    Json::Value TicketService::accountSecurityStatus(const Json::Value &req)
    {
        const std::string token = req.get(field::kToken, "").asString();
        std::string tel;
        int64_t userId = 0;
        if (token.empty() || !sessions_->resolve(token, nowMs(), tel, userId))
            return makeError(err::kUnauthorized);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        User user;
        if (!userRepo_.findByTel(conn, tel, user) || user.id != userId || user.status != 1)
            return makeError(err::kUnauthorized);

        Json::Value response = makeOk();
        response[field::kUserTel] = user.tel;
        response[field::kEmail] = user.email;
        response["email_verified"] = user.emailVerified;
        response["phone_verified"] = user.phoneVerified;
        return response;
    }

    Json::Value TicketService::requestContactVerification(const Json::Value &req)
    {
        if (!verificationSender_) return makeError("VERIFICATION_UNAVAILABLE");
        const std::string token = req.get(field::kToken, "").asString();
        const std::string password = req.get(field::kPassword, "").asString();
        const std::string channel = req.get(field::kChannel, "").asString();
        std::string tel;
        int64_t userId = 0;
        if (token.empty() || !sessions_->resolve(token, nowMs(), tel, userId))
            return makeError(err::kUnauthorized);
        if (channel != "EMAIL" && channel != "SMS")
            return makeError(err::kInvalidInput);
        if (channel == "SMS" && !mockSmsEnabled_)
            return makeError("VERIFICATION_CHANNEL_UNAVAILABLE");

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        User user;
        if (!userRepo_.findByTel(conn, tel, user) || user.id != userId || user.status != 1)
            return makeError(err::kUnauthorized);

        int64_t reauthRetryAfter = 0;
        if (!authRepo_.isBlocked(conn, "contact_reauth", tel, reauthRetryAfter))
            return makeError(err::kDbUnavailable);
        if (reauthRetryAfter > 0)
        {
            Json::Value locked = makeError("AUTH_TEMPORARILY_LOCKED");
            locked["retry_after_seconds"] = static_cast<Json::Int64>(reauthRetryAfter);
            return locked;
        }
        bool needRehash = false;
        if (!verifyPassword(password, user.passwordHash, needRehash))
        {
            authRepo_.recordFailure(conn, "contact_reauth", tel,
                                    authFailureWindowSeconds_, authMaxFailures_, authLockSeconds_);
            authRepo_.audit(conn, "user", tel, "CONTACT_VERIFICATION_DENIED",
                            req.get("_client_ip", "").asString(), "invalid_password");
            return makeError(err::kInvalidCredentials);
        }
        if (needRehash) userRepo_.updatePasswordHash(conn, tel, hashPassword(password));
        authRepo_.clear(conn, "contact_reauth", tel);

        const std::string destination = channel == "EMAIL"
            ? req.get(field::kEmail, "").asString() : tel;
        if ((channel == "EMAIL" && !validEmail(destination)) ||
            (channel == "SMS" && destination.size() != 11))
            return makeError(err::kInvalidInput);

        if (channel == "EMAIL")
        {
            User owner;
            if (userRepo_.findByTelOrEmail(conn, destination, owner) && owner.id != userId)
                return makeError("CONTACT_ALREADY_IN_USE");
        }

        const std::string ip = req.get("_client_ip", "").asString();
        const std::string device = req.get("client_id", "").asString().substr(0, 128);
        int64_t retryAfter = 0;
        if ((!ip.empty() && (!authRepo_.isBlocked(conn, "verification_ip", ip, retryAfter) || retryAfter > 0)) ||
            (!device.empty() && (!authRepo_.isBlocked(conn, "verification_device", device, retryAfter) || retryAfter > 0)) ||
            !authRepo_.isBlocked(conn, "verification_destination", destination, retryAfter) || retryAfter > 0)
            return makeError("VERIFICATION_RATE_LIMITED");
        if (!challengeRepo_.canSend(conn, "CONTACT", channel, destination,
                                    resendCooldownSeconds_))
            return makeError("VERIFICATION_COOLDOWN");

        const std::string id = secureRandomHex();
        const std::string code = generateNumericCode();
        if (!challengeRepo_.insert(conn, id, userId, tel, destination, "CONTACT", channel,
                                   hashPassword(code), codeTtlSeconds_, verificationMaxAttempts_))
            return makeError(err::kDbInsert);

        std::string deliveryError;
        const bool delivered = channel == "EMAIL"
            ? verificationSender_->sendEmailCode(destination, code, "CONTACT", deliveryError)
            : verificationSender_->sendMockSmsCode(destination, code, "CONTACT", deliveryError);
        if (!delivered)
        {
            challengeRepo_.remove(conn, id);
            authRepo_.audit(conn, "user", tel, "CONTACT_VERIFICATION_DELIVERY_FAILED",
                            ip, deliveryError);
            return makeError("VERIFICATION_DELIVERY_FAILED");
        }

        authRepo_.audit(conn, "user", tel, "CONTACT_VERIFICATION_SENT", ip, channel);
        if (!ip.empty()) authRepo_.recordFailure(conn, "verification_ip", ip, 900, 20, 900);
        if (!device.empty()) authRepo_.recordFailure(conn, "verification_device", device, 900, 10, 900);
        authRepo_.recordFailure(conn, "verification_destination", destination, 86400, 10, 86400);
        Json::Value response = challengeResponse(id, codeTtlSeconds_);
        response[field::kChannel] = channel;
        if (channel == "SMS" && exposeMockSmsCode_) response["mock_code"] = code;
        return response;
    }

    Json::Value TicketService::confirmContactVerification(const Json::Value &req)
    {
        const std::string token = req.get(field::kToken, "").asString();
        const std::string id = req.get(field::kChallengeId, "").asString();
        const std::string code = req.get(field::kCode, "").asString();
        std::string tel;
        int64_t userId = 0;
        if (token.empty() || !sessions_->resolve(token, nowMs(), tel, userId))
            return makeError(err::kUnauthorized);
        if (id.size() != 64 || code.size() != 6)
            return makeError(err::kInvalidInput);

        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        if (!conn) return makeError(err::kDbUnavailable);
        Txn txn(conn);
        AuthChallenge challenge;
        if (!txn.ok() || !challengeRepo_.lockActive(conn, id, "CONTACT", challenge) ||
            challenge.userId != userId || challenge.subject != tel)
        {
            txn.rollback();
            return makeError("INVALID_OR_EXPIRED_CODE");
        }

        bool rehash = false;
        if (!verifyPassword(code, challenge.codeHash, rehash))
        {
            challengeRepo_.recordFailedAttempt(conn, id);
            txn.commit();
            return makeError("INVALID_OR_EXPIRED_CODE");
        }

        bool updated = false;
        if (challenge.channel == "EMAIL")
        {
            User owner;
            if (userRepo_.findByTelOrEmail(conn, challenge.destination, owner) &&
                owner.id != userId)
            {
                txn.rollback();
                return makeError("CONTACT_ALREADY_IN_USE");
            }
            updated = userRepo_.bindVerifiedEmail(conn, userId, challenge.destination);
        }
        else if (challenge.channel == "SMS" && challenge.destination == tel)
        {
            updated = userRepo_.markPhoneVerified(conn, userId, tel);
        }
        if (!updated || !challengeRepo_.consume(conn, id))
        {
            txn.rollback();
            return makeError("CONTACT_UPDATE_FAILED");
        }

        authRepo_.audit(conn, "user", tel, "CONTACT_VERIFIED",
                        req.get("_client_ip", "").asString(), challenge.channel);
        if (!txn.commit()) return makeError(err::kDbUpdate);

        User updatedUser;
        if (!userRepo_.findByTel(conn, tel, updatedUser))
            return makeError(err::kDbUnavailable);
        Json::Value response = makeOk();
        response[field::kChannel] = challenge.channel;
        response[field::kEmail] = updatedUser.email;
        response["email_verified"] = updatedUser.emailVerified;
        response["phone_verified"] = updatedUser.phoneVerified;
        return response;
    }

    bool TicketService::purgeAuthenticationState()
    {
        MYSQL *conn = nullptr;
        shanchuan::ConnectionGuard guard(&conn, pool_);
        return conn && challengeRepo_.purgeExpired(conn) && authRepo_.purgeOldThrottles(conn);
    }
}
