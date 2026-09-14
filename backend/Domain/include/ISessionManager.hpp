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

        // 撤销一个用户 token；密码重置等安全事件可按用户撤销全部 token。
        virtual void remove(const std::string &token) = 0;
        virtual void removeAllForUser(int64_t userId) = 0;

        // 管理员会话与用户会话使用独立命名空间。mustChangePassword=true 时，
        // 该 token 只能用于修改默认密码，不能执行其他管理操作。
        virtual std::string createAdmin(const std::string &username,
                                        bool mustChangePassword,
                                        int64_t nowMs) = 0;
        virtual bool resolveAdmin(const std::string &token, int64_t nowMs,
                                  std::string &usernameOut,
                                  bool &mustChangePasswordOut) = 0;
        virtual void removeAdmin(const std::string &token) = 0;
        virtual void removeAllForAdmin(const std::string &username) = 0;

        // 定时清理过期 token。
        virtual void purgeExpired(int64_t nowMs) = 0;
    };
} // namespace hyperticket
#endif // HYPERTICKET_ISESSION_MANAGER_HPP
