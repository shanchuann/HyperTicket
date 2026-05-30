#include "ConnectionPool.hpp"
#include "Logger.hpp"

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
        for (int i = 0; i < maxconn; ++i)
        {
            MYSQL *con = mysql_init(nullptr);
            if (nullptr == con)
            {
                LOG_ERROR << "mysql_init error";
                exit(1);
            }

            con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(),
                                     databasename.c_str(), port, nullptr, 0);
            if (nullptr == con)
            {
                LOG_ERROR << "mysql_real_connect error";
                exit(1);
            }
            connList.push_back(con);
            ++m_FreeConn;
        }
        sem_reserve.reset(m_FreeConn);
        m_MaxConn = m_FreeConn;
        LOG_INFO << "ConnectionPool initialized with " << m_MaxConn << " connections";
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
