#ifndef HYPERTICKET_USER_REPOSITORY_HPP
#define HYPERTICKET_USER_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // users 表的数据访问。所有查询用预处理语句参数化，接受调用方提供的 MYSQL*
    // （ser 走连接池借出、admin 用裸连接），不持有连接，便于在事务内外复用。
    class UserRepository
    {
    public:
        // 按手机号查询单个用户；未找到返回 false。
        bool findByTel(MYSQL *conn, const std::string &tel, User &out)
        {
            MysqlStmt st(conn, "SELECT id, username, password_hash, status FROM users WHERE tel = ?");
            if (!st.ok()) return false;
            st.bindString(0, tel);
            if (!st.execute() || !st.bindResults(4)) return false;
            if (!st.fetch()) return false;
            out.id = st.getInt(0);
            out.tel = tel;
            out.username = st.getString(1);
            out.passwordHash = st.getString(2);
            out.status = static_cast<int>(st.getInt(3));
            return true;
        }

        // 插入新用户（status=1 正常）。
        bool insert(MYSQL *conn, const std::string &tel, const std::string &username,
                    const std::string &passwordHash)
        {
            MysqlStmt st(conn, "INSERT INTO users (tel, username, password_hash, status) VALUES(?,?,?,1)");
            if (!st.ok()) return false;
            st.bindString(0, tel);
            st.bindString(1, username);
            st.bindString(2, passwordHash);
            return st.execute();
        }

        // 列出全部用户（admin 查看所有用户）。
        std::vector<User> listAll(MYSQL *conn)
        {
            return query(conn, "SELECT id, tel, username, password_hash, status FROM users");
        }

        // 列出指定状态用户（admin 查看黑名单 status=0）。
        std::vector<User> listByStatus(MYSQL *conn, int status)
        {
            std::vector<User> out;
            MysqlStmt st(conn, "SELECT id, tel, username, password_hash, status FROM users WHERE status = ?");
            if (!st.ok()) return out;
            st.bindInt(0, status);
            if (!st.execute() || !st.bindResults(5)) return out;
            fill(st, out);
            return out;
        }

        // 查询用户当前状态；未找到返回 false。
        bool getStatusByTel(MYSQL *conn, const std::string &tel, int &statusOut)
        {
            MysqlStmt st(conn, "SELECT status FROM users WHERE tel = ?");
            if (!st.ok()) return false;
            st.bindString(0, tel);
            if (!st.execute() || !st.bindResults(1)) return false;
            if (!st.fetch()) return false;
            statusOut = static_cast<int>(st.getInt(0));
            return true;
        }

        // 设置用户状态（加入/移出黑名单）。
        bool setStatusByTel(MYSQL *conn, const std::string &tel, int status)
        {
            MysqlStmt st(conn, "UPDATE users SET status = ? WHERE tel = ?");
            if (!st.ok()) return false;
            st.bindInt(0, status);
            st.bindString(1, tel);
            return st.execute();
        }

    private:
        std::vector<User> query(MYSQL *conn, const std::string &sql)
        {
            std::vector<User> out;
            MysqlStmt st(conn, sql);
            if (!st.ok() || !st.execute() || !st.bindResults(5)) return out;
            fill(st, out);
            return out;
        }
        void fill(MysqlStmt &st, std::vector<User> &out)
        {
            while (st.fetch())
            {
                User u;
                u.id = st.getInt(0);
                u.tel = st.getString(1);
                u.username = st.getString(2);
                u.passwordHash = st.getString(3);
                u.status = static_cast<int>(st.getInt(4));
                out.push_back(u);
            }
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_USER_REPOSITORY_HPP
