#include "ConnectionPool.hpp"
#include "Logger.hpp"
#include <stdexcept>
#include <chrono>

namespace shanchuan
{
    MYSQL *ConnectionPool::GetConnection()
    {
        return GetConnectionWithTimeout(1000);
    }

    MYSQL *ConnectionPool::GetConnectionWithTimeout(int timeoutMs)
    {
        MYSQL *con = nullptr;
        bool rebuildLostSlot = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_available.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
                    return !connList.empty() || m_LostConn > 0;
                }))
            {
                LOG_WARN << "GetConnection timeout after " << timeoutMs << "ms";
                return nullptr;
            }
            if (!connList.empty())
            {
                con = connList.front();
                connList.pop_front();
                --m_FreeConn;
            }
            else
            {
                --m_LostConn;
                rebuildLostSlot = true;
            }
        }

        if (rebuildLostSlot)
        {
            con = createConnection();
        }
        else if (!ping(con))
        {
            LOG_WARN << "Connection lost, replacing handle";
            con = reconnect(con);
        }

        if (!con)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_LostConn;
            m_available.notify_one();
            return nullptr;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_CurConn;
        }
        return con;
    }

    bool ConnectionPool::ping(MYSQL *conn)
    {
        if (!conn) return false;
        // mysql_ping 返回0表示连接正常，非0表示连接断开
        return mysql_ping(conn) == 0;
    }

    MYSQL *ConnectionPool::reconnect(MYSQL *conn)
    {
        if (conn) mysql_close(conn);
        MYSQL *replacement = createConnection();
        if (!replacement) LOG_ERROR << "Failed to replace disconnected MySQL handle";
        return replacement;
    }

    void ConnectionPool::healthCheck()
    {
        size_t idleCount = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            idleCount = connList.size();
        }
        LOG_INFO << "Running health check on " << idleCount << " idle connections";

        int reconnected = 0;
        for (size_t i = 0; i < idleCount; ++i)
        {
            MYSQL *conn = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (connList.empty()) break;
                conn = connList.front();
                connList.pop_front();
                --m_FreeConn;
                ++m_CurConn;
            }

            if (!ping(conn))
            {
                LOG_WARN << "Idle connection lost, reconnecting...";
                conn = reconnect(conn);
                if (conn) ++reconnected;
                else LOG_ERROR << "Failed to reconnect idle connection";
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                --m_CurConn;
                if (conn)
                {
                    connList.push_back(conn);
                    ++m_FreeConn;
                }
                else ++m_LostConn;
            }
            m_available.notify_one();
        }

        if (reconnected > 0)
        {
            LOG_INFO << "Health check completed: " << reconnected << " connections reconnected";
        }
    }

    MYSQL *ConnectionPool::createConnection()
    {
        MYSQL *handle = mysql_init(nullptr);
        if (!handle)
        {
            LOG_ERROR << "mysql_init error";
            return nullptr;
        }

        // 设置连接选项
        unsigned int timeout = 5;  // 5秒超时
        mysql_options(handle, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(handle, MYSQL_OPT_READ_TIMEOUT, &timeout);
        mysql_options(handle, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

        // 启用自动重连（可选，但建议应用层处理）
        // my_bool reconnect = 1;
        // mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);

        MYSQL *connected = mysql_real_connect(handle, m_url.c_str(), m_user.c_str(), m_password.c_str(),
                                             m_databasename.c_str(), m_port, nullptr, 0);
        if (!connected)
        {
            LOG_ERROR << "mysql_real_connect error: " << mysql_error(handle);
            mysql_close(handle);
            return nullptr;
        }

        return connected;
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
            if (m_CurConn > 0) --m_CurConn;
        }
        m_available.notify_one();
        return true;
    }

    int ConnectionPool::GetFreeConn() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_FreeConn;
    }

    int ConnectionPool::GetActiveConn() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
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
        m_LostConn = 0;
        connList.clear();
        m_available.notify_all();
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

        std::list<MYSQL *> created;
        for (int i = 0; i < maxconn; ++i)
        {
            MYSQL *con = createConnection();
            if (!con)
            {
                LOG_ERROR << "Failed to create connection " << (i + 1) << "/" << maxconn;
                for (MYSQL *existing : created) mysql_close(existing);
                throw std::runtime_error("Failed to initialize connection pool");
            }
            created.push_back(con);
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            connList.swap(created);
            m_FreeConn = static_cast<int>(connList.size());
            m_CurConn = 0;
            m_LostConn = 0;
            m_MaxConn = m_FreeConn;
        }
        m_available.notify_all();
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

    ConnectionGuard::ConnectionGuard(MYSQL **SQL, ConnectionPool *connPool, int timeoutMs)
    {
        *SQL = connPool->GetConnectionWithTimeout(timeoutMs);
        connRAII = *SQL;
        poolRAII = connPool;
    }

    ConnectionGuard::~ConnectionGuard()
    {
        poolRAII->ReleaseConnection(connRAII);
    }
} // namespace shanchuan
