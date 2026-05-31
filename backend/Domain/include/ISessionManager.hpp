#ifndef HYPERTICKET_ISESSION_MANAGER_HPP
#define HYPERTICKET_ISESSION_MANAGER_HPP

#include <cstdint>
#include <string>

namespace hyperticket
{
    // 会话管理器接口：TicketService 依赖此接口而非具体实现，
    // 解除 Domain 层对 Server 层的反向依赖。
    class ISessionManager
    {
    public:
        virtual ~ISessionManager() = default;

        // 生成并存储一个新 token，返回该 token。
        virtual std::string create(const std::string &tel, int64_t userId, int64_t nowMs) = 0;

        // 校验 token：有效则填充 tel/userId 并续期，返回 true。
        virtual bool resolve(const std::string &token, int64_t nowMs,
                             std::string &telOut, int64_t &userIdOut) = 0;

        // 定时清理过期 token。
        virtual void purgeExpired(int64_t nowMs) = 0;
    };
} // namespace hyperticket
#endif // HYPERTICKET_ISESSION_MANAGER_HPP
