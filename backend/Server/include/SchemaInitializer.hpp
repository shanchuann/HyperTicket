#ifndef HYPERTICKET_SCHEMA_INITIALIZER_HPP
#define HYPERTICKET_SCHEMA_INITIALIZER_HPP

#include <algorithm>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>

#include <mysql/mysql.h>

#include "../../Common/include/AppConfig.hpp"
#include "../../ChronoLite/include/Logger.hpp"

namespace hyperticket
{
    // 数据库启动闸门：只验证连接和 schema 版本，不在服务启动时改表。
    // 新库使用 scripts/bootstrap-schema.sh，已有 v10 库应用 v11 迁移。
    class SchemaInitializer
    {
    public:
        static constexpr int kExpectedSchemaVersion = 11;

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

            // 校验数据库名：仅允许字母/数字/下划线，防止 SQL 注入。
            if (db.name.empty() ||
                !std::all_of(db.name.begin(), db.name.end(),
                             [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; }))
            {
                LOG_ERROR << "invalid database name: " << db.name;
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
                mysql_close(conn);
                return false;
            }

            if (mysql_query(conn, "SELECT COALESCE(MAX(version),0) FROM schema_migrations") != 0)
            {
                LOG_ERROR << "schema version unavailable; apply db/migrate_v11_schema_version.sql: "
                          << mysql_error(conn);
                mysql_close(conn);
                return false;
            }
            MYSQL_RES *result = mysql_store_result(conn);
            MYSQL_ROW row = result ? mysql_fetch_row(result) : nullptr;
            int version = 0;
            try
            {
                version = row && row[0] ? std::stoi(row[0]) : 0;
            }
            catch (const std::exception &error)
            {
                LOG_ERROR << "invalid schema version value: " << error.what();
                if (result) mysql_free_result(result);
                mysql_close(conn);
                return false;
            }
            if (result) mysql_free_result(result);
            if (version != kExpectedSchemaVersion)
            {
                LOG_ERROR << "schema version mismatch: expected " << kExpectedSchemaVersion
                          << ", found " << version;
                mysql_close(conn);
                return false;
            }
            if (mysql_query(conn,
                    "SELECT COUNT(*) FROM information_schema.COLUMNS "
                    "WHERE TABLE_SCHEMA=DATABASE() AND "
                    "((TABLE_NAME='payments' AND COLUMN_NAME='action_token') OR "
                    "(TABLE_NAME='refunds' AND COLUMN_NAME='action_token'))") != 0)
            {
                LOG_ERROR << "schema compatibility check failed: " << mysql_error(conn);
                mysql_close(conn);
                return false;
            }
            result = mysql_store_result(conn);
            row = result ? mysql_fetch_row(result) : nullptr;
            const bool hasActionTokens = row && row[0] && std::string(row[0]) == "2";
            if (result) mysql_free_result(result);
            if (!hasActionTokens)
            {
                LOG_ERROR << "schema v11 is incomplete; rerun db/migrate_v11_schema_version.sql";
                mysql_close(conn);
                return false;
            }
            mysql_close(conn);
            return true;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_SCHEMA_INITIALIZER_HPP
