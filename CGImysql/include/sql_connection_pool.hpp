
#include <string>
#include <list>
#include <mutex>

using namespace std;

#include <mysql/mysql.h>

#include"Sem.hpp"
#ifndef SQL_CONNECTION_HPP
#define SQL_CONNECTION_HPP

namespace tulun
{

   
    class connection_pool
    {

    public:
        std::string m_url;          // 主机地址
        std::string m_port;         // 数据库端口号
        std::string m_user;         // 登陆数据库用户名
        std::string m_password;     // 登陆数据库密码
        std::string m_databasename; // 使用数据库名
        int m_close_log;            // 日志开关
    public:
        MYSQL *GetConnection();
        bool ReleaseConnection(MYSQL *conn);
        int GetFreeConn() const;
        void DestroyPool();
        void init(const std::string &url, const std::string &user, const std::string password,
                  std::string databasename, int port, int maxconn, int close_log);

        static connection_pool *GetInstance();

    private:
        connection_pool();
        ~connection_pool();

        int m_MaxConn;
        int m_CurConn;
        int m_FreeConn;
        std::mutex m_mutex;
        std::list<MYSQL *> connList;
        sem sem_reserve;
    };

    class connectionRAII
    {
    private:
        MYSQL *connRAII;
        connection_pool *poolRAII;

    public:
        connectionRAII(MYSQL **con, connection_pool *connPool);
        ~connectionRAII();
    };
} // namespace tulun

#endif