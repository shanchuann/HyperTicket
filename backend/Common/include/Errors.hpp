#ifndef HYPERTICKET_ERRORS_HPP
#define HYPERTICKET_ERRORS_HPP

namespace hyperticket
{
    // 统一的错误码常量（makeError 的 reason 取值），集中定义避免分散的字符串字面量。
    namespace err
    {
        constexpr const char *kInvalidInput = "INVALID_INPUT";
        constexpr const char *kUnknownType = "UNKNOWN_TYPE";
        constexpr const char *kUnauthorized = "UNAUTHORIZED";

        constexpr const char *kDbUnavailable = "DB_UNAVAILABLE";
        constexpr const char *kDbQuery = "DB_QUERY";
        constexpr const char *kDbResult = "DB_RESULT";
        constexpr const char *kDbInsert = "DB_INSERT";
        constexpr const char *kDbUpdate = "DB_UPDATE";
        constexpr const char *kDbBegin = "DB_BEGIN";
        constexpr const char *kRateLimited = "RATE_LIMITED";

        constexpr const char *kUserNotFound = "USER_NOT_FOUND";
        constexpr const char *kPasswdError = "PASSWD_ERROR";
        constexpr const char *kInvalidCredentials = "INVALID_CREDENTIALS"; // 统一：用户不存在或密码错误
        constexpr const char *kWeakPassword = "WEAK_PASSWORD";
        constexpr const char *kBlacklisted = "BLACKLISTED";

        constexpr const char *kTicketNotFound = "TICKET_NOT_FOUND";
        constexpr const char *kTicketOffline = "TICKET_OFFLINE";
        constexpr const char *kNoTicket = "NO_TICKET";

        constexpr const char *kOrderNotFound = "ORDER_NOT_FOUND";
        constexpr const char *kOrderCannotCancel = "ORDER_CANNOT_CANCEL";
    }
} // namespace hyperticket
#endif // HYPERTICKET_ERRORS_HPP
