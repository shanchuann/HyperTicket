#ifndef CONNECTIONPOOL_HPP
#define CONNECTIONPOOL_HPP

#include <string>
#include <list>
#include <mutex>
#include <chrono>
#include <mysql/mysql.h>
#include "Semaphore.hpp"

namespace shanchuan
{
    // A fixed-size pool of reusable MySQL connections. Connections are created up
    // front in init() and handed out / returned through Get/ReleaseConnection.
    // Prefer ConnectionGuard for scoped acquisition.
    class ConnectionPool
    {
    public:
        MYSQL *GetConnection();
        MYSQL *GetConnectionWithTimeout(int timeoutMs);  // 新增：带超时的获取连接
        bool ReleaseConnection(MYSQL *conn);
        int GetFreeConn() const;
        int GetActiveConn() const;  // 新增：获取活跃连接数
        void DestroyPool();
        void init(const std::string &url, const std::string &user, const std::string &password,
                  const std::string &databasename, int port, int maxconn, int close_log);
        static ConnectionPool *GetInstance();

        // 新增：连接健康检查和维护
        bool ping(MYSQL *conn);           // 检查连接是否存活
        bool reconnect(MYSQL *conn);      // 重连断开的连接
        void healthCheck();               // 定期健康检查所有空闲连接

    private:
        ConnectionPool();
        ~ConnectionPool();
        ConnectionPool(const ConnectionPool &) = delete;
        ConnectionPool &operator=(const ConnectionPool &) = delete;

        MYSQL *createConnection();  // 新增：创建单个连接的辅助方法

        std::string m_url;          // 主机地址
        int m_port = 0;             // 数据库端口号
        std::string m_user;         // 登陆数据库用户名
        std::string m_password;     // 登陆数据库密码
        std::string m_databasename; // 使用数据库名
        int m_close_log = 0;        // 日志开关

        int m_MaxConn = 0;
        int m_CurConn = 0;
        int m_FreeConn = 0;
        std::mutex m_mutex;
        std::list<MYSQL *> connList;
        Semaphore sem_reserve;
    };

    // RAII helper: borrows a connection on construction, returns it on destruction.
    class ConnectionGuard
    {
    public:
        ConnectionGuard(MYSQL **con, ConnectionPool *connPool);
        ~ConnectionGuard();
        ConnectionGuard(const ConnectionGuard &) = delete;
        ConnectionGuard &operator=(const ConnectionGuard &) = delete;

    private:
        MYSQL *connRAII;
        ConnectionPool *poolRAII;
    };
} // namespace shanchuan
#endif // CONNECTIONPOOL_HPP
