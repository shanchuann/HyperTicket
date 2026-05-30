#ifndef HYPERTICKET_SCHEMA_INITIALIZER_HPP
#define HYPERTICKET_SCHEMA_INITIALIZER_HPP

#include <fstream>
#include <sstream>
#include <string>

#include <mysql/mysql.h>

#include "../../Common/include/AppConfig.hpp"
#include "../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    // 数据库初始化：确保配置的库可连接、可使用。schema 的唯一来源是 db/init.sql，
    // 由运维手动执行；此处只验证库可用（与原 ensureDatabaseAndTables 行为一致），
    // 不在启动时自动改表结构，避免与 init.sql 产生分歧。
    class SchemaInitializer
    {
    public:
        // 返回 true 表示数据库已就绪可用。
        static bool ensureReady(const DbConfig &db)
        {
            MYSQL *conn = mysql_init(nullptr);
            if (!conn)
            {
                LOG_ERROR << "mysql_init failed";
                return false;
            }
            // 先不指定库名连接，便于建库（若权限允许）。
            if (!mysql_real_connect(conn, db.host.c_str(), db.user.c_str(),
                                    db.password.c_str(), nullptr, db.port, nullptr, 0))
            {
                LOG_ERROR << "mysql_real_connect failed: " << mysql_error(conn);
                mysql_close(conn);
                return false;
            }

            std::string createDb =
                "CREATE DATABASE IF NOT EXISTS " + db.name + " CHARACTER SET utf8mb4";
            bool ok = (mysql_query(conn, createDb.c_str()) == 0) &&
                      (mysql_query(conn, ("USE " + db.name).c_str()) == 0);
            if (!ok)
            {
                LOG_ERROR << "ensure database failed: " << mysql_error(conn);
            }
            mysql_close(conn);
            return ok;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_SCHEMA_INITIALIZER_HPP
