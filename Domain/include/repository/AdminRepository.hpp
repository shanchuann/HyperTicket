#ifndef HYPERTICKET_ADMIN_REPOSITORY_HPP
#define HYPERTICKET_ADMIN_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // admins 表的数据访问。所有查询用预处理语句参数化，接受调用方提供的 MYSQL*。
    class AdminRepository
    {
    public:
        // 按用户名查询单个管理员；未找到返回 false。
        bool findByUsername(MYSQL *conn, const std::string &username, Admin &out)
        {
            MysqlStmt st(conn, "SELECT id, username, password_hash, role FROM admins WHERE username = ?");
            if (!st.ok()) return false;
            st.bindString(0, username);
            if (!st.execute() || !st.bindResults(4)) return false;
            if (!st.fetch()) return false;
            out.id = st.getInt(0);
            out.username = st.getString(1);
            out.passwordHash = st.getString(2);
            out.role = st.getString(3);
            return true;
        }

        // 更新管理员密码哈希（用于旧哈希自动迁移）。
        bool updatePasswordHash(MYSQL *conn, const std::string &username, const std::string &newHash)
        {
            MysqlStmt st(conn, "UPDATE admins SET password_hash = ? WHERE username = ?");
            if (!st.ok()) return false;
            st.bindString(0, newHash);
            st.bindString(1, username);
            return st.execute();
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_ADMIN_REPOSITORY_HPP
