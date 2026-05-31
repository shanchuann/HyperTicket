#include "ConnectionPool.hpp"
#include "Logger.hpp"
#include <stdexcept>
#include <chrono>
#include <thread>

namespace shanchuan
{
    MYSQL *ConnectionPool::GetConnection()
    {
        // Block until a connection is available; the semaphore tracks free slots,
        // so we must not pre-check connList without the lock (race).
        sem_reserve.wait();
        std::lock_guard<std::mutex> lock(m_mutex);
        if (connList.empty())
        {
            sem_reserve.post();
            return nullptr;
        }
        MYSQL *con = connList.front();
        connList.pop_front();
        --m_FreeConn;
        ++m_CurConn;

        // 健康检查：如果连接已断开，尝试重连
        if (!ping(con))
        {
            LOG_WARN << "Connection lost, attempting reconnect...";
            if (!reconnect(con))
            {
                LOG_ERROR << "Reconnect failed, connection unusable";
                // 连接无法恢复，返回池中并返回nullptr
                connList.push_back(con);
                ++m_FreeConn;
                --m_CurConn;
                sem_reserve.post();
                return nullptr;
            }
            LOG_INFO << "Reconnect successful";
        }

        return con;
    }

    MYSQL *ConnectionPool::GetConnectionWithTimeout(int timeoutMs)
    {
        // 带超时的获取连接
        auto start = std::chrono::steady_clock::now();

        while (true)
        {
            // 尝试获取信号量（非阻塞）
            if (sem_reserve.try_wait())
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (connList.empty())
                {
                    sem_reserve.post();
                    return nullptr;
                }
                MYSQL *con = connList.front();
                connList.pop_front();
                --m_FreeConn;
                ++m_CurConn;

                // 健康检查
                if (!ping(con))
                {
                    LOG_WARN << "Connection lost, attempting reconnect...";
                    if (!reconnect(con))
                    {
                        LOG_ERROR << "Reconnect failed";
                        connList.push_back(con);
                        ++m_FreeConn;
                        --m_CurConn;
                        sem_reserve.post();
                        return nullptr;
                    }
                }

                return con;
            }

            // 检查超时
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= timeoutMs)
            {
                LOG_WARN << "GetConnection timeout after " << timeoutMs << "ms";
                return nullptr;
            }

            // 短暂休眠后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    bool ConnectionPool::ping(MYSQL *conn)
    {
        if (!conn) return false;
        // mysql_ping 返回0表示连接正常，非0表示连接断开
        return mysql_ping(conn) == 0;
    }

    bool ConnectionPool::reconnect(MYSQL *conn)
    {
        if (!conn) return false;

        // 关闭旧连接
        mysql_close(conn);

        // 重新初始化
        conn = mysql_init(conn);
        if (!conn)
        {
            LOG_ERROR << "mysql_init failed during reconnect";
            return false;
        }

        // 设置连接超时
        unsigned int timeout = 5;  // 5秒超时
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
        mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

        // 重新连接
        MYSQL *result = mysql_real_connect(conn, m_url.c_str(), m_user.c_str(),
                                          m_password.c_str(), m_databasename.c_str(),
                                          m_port, nullptr, 0);
        if (!result)
        {
            LOG_ERROR << "mysql_real_connect failed during reconnect: " << mysql_error(conn);
            return false;
        }

        return true;
    }

    void ConnectionPool::healthCheck()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        LOG_INFO << "Running health check on " << connList.size() << " idle connections";

        int reconnected = 0;
        for (auto it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *conn = *it;
            if (!ping(conn))
            {
                LOG_WARN << "Idle connection lost, reconnecting...";
                if (reconnect(conn))
                {
                    ++reconnected;
                }
                else
                {
                    LOG_ERROR << "Failed to reconnect idle connection";
                }
            }
        }

        if (reconnected > 0)
        {
            LOG_INFO << "Health check completed: " << reconnected << " connections reconnected";
        }
    }

    MYSQL *ConnectionPool::createConnection()
    {
        MYSQL *con = mysql_init(nullptr);
        if (!con)
        {
            LOG_ERROR << "mysql_init error";
            return nullptr;
        }

        // 设置连接选项
        unsigned int timeout = 5;  // 5秒超时
        mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &timeout);
        mysql_options(con, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

        // 启用自动重连（可选，但建议应用层处理）
        // my_bool reconnect = 1;
        // mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);

        con = mysql_real_connect(con, m_url.c_str(), m_user.c_str(), m_password.c_str(),
                                m_databasename.c_str(), m_port, nullptr, 0);
        if (!con)
        {
            LOG_ERROR << "mysql_real_connect error: " << mysql_error(con);
            return nullptr;
        }

        return con;
    }

    bool ConnectionPool::ReleaseConnection(MYSQL *conn)
    {
        if (nullptr == conn)
        {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            connList.push_back(conn);
            ++m_FreeConn;
            --m_CurConn;
        }
        sem_reserve.post();
        return true;
    }

    int ConnectionPool::GetFreeConn() const
    {
        return m_FreeConn;
    }

    int ConnectionPool::GetActiveConn() const
    {
        return m_CurConn;
    }

    void ConnectionPool::DestroyPool()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (MYSQL *con : connList)
        {
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    void ConnectionPool::init(const std::string &url, const std::string &user, const std::string &password,
                              const std::string &databasename, int port, int maxconn, int close_log)
    {
        m_url = url;
        m_port = port;
        m_user = user;
        m_password = password;
        m_databasename = databasename;
        m_close_log = close_log;

        LOG_INFO << "Initializing connection pool with " << maxconn << " connections...";

        for (int i = 0; i < maxconn; ++i)
        {
            MYSQL *con = createConnection();
            if (!con)
            {
                LOG_ERROR << "Failed to create connection " << (i + 1) << "/" << maxconn;
                throw std::runtime_error("Failed to initialize connection pool");
            }
            connList.push_back(con);
            ++m_FreeConn;
        }

        sem_reserve.reset(m_FreeConn);
        m_MaxConn = m_FreeConn;
        LOG_INFO << "ConnectionPool initialized successfully: "
                 << m_MaxConn << " connections to " << m_url << ":" << m_port
                 << "/" << m_databasename;
    }

    ConnectionPool *ConnectionPool::GetInstance()
    {
        static ConnectionPool connPool;
        return &connPool;
    }

    ConnectionPool::ConnectionPool() = default;

    ConnectionPool::~ConnectionPool()
    {
        DestroyPool();
    }

    ConnectionGuard::ConnectionGuard(MYSQL **SQL, ConnectionPool *connPool)
    {
        *SQL = connPool->GetConnection();
        connRAII = *SQL;
        poolRAII = connPool;
    }

    ConnectionGuard::~ConnectionGuard()
    {
        poolRAII->ReleaseConnection(connRAII);
    }
} // namespace shanchuan
