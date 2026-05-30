

#include "log.hpp"
#include "sql_connection_pool.hpp"
#include "Logger.hpp"

namespace tulun
{
    // connection_pool
    // std::string m_url;          // 主机地址
    // std::string m_port;         // 数据库端口号
    // std::string m_user;         // 登陆数据库用户名
    // std::string m_password;     // 登陆数据库密码
    // std::string m_databasename; // 使用数据库名
    // int m_close_log;            // 日志开关

    MYSQL *connection_pool::GetConnection()
    {
        MYSQL *con = nullptr;
        if (0 == connList.size())
        {
            return nullptr;
        }
        sem_reserve.wait();
        std::lock_guard<std::mutex> lock(m_mutex);
        con = connList.front();
        connList.pop_front();
        --m_FreeConn;
        ++m_CurConn;
        return con;
    }
    bool connection_pool::ReleaseConnection(MYSQL *conn)
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
    int connection_pool::GetFreeConn() const
    {
        return m_FreeConn;
    }
    void connection_pool::DestroyPool()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (connList.size() > 0)
        {
            std::list<MYSQL *>::iterator it;
            for (it = connList.begin(); it != connList.end(); ++it)
            {
                MYSQL *con = *it;
                mysql_close(con);
            }
            m_CurConn = 0;
            m_FreeConn = 0;
            connList.clear();
        }
    }
    void connection_pool::init(const std::string &url, const std::string &user, const std::string password,
                               std::string databasename, int port, int maxconn, int close_log)
    {
        m_url = url;
        m_port = port;
        m_user = user;
        m_password = password;
        m_databasename = databasename;
        m_close_log = close_log;
        for (int i = 0; i < maxconn; ++i)
        {
            MYSQL *con = nullptr;
            con = mysql_init(NULL);
            if (nullptr == con)
            {
                LOG_ERROR<<"mySql init Error ";
                exit(1);
            }
            else
            {
                LOG_INFO<< "con : "<< con;
            }

            con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(), databasename.c_str(), port, nullptr, 0);
            if (nullptr == con)
            {
                LOG_ERROR <<"mysql_real_connect mysql error "<<con;
                exit(1);
            }
            connList.push_back(con);
            ++m_FreeConn;
            LOG_TRACE<<"m_FreeConn : "<<m_FreeConn;
        }
        sem_reserve = sem(m_FreeConn);
        m_MaxConn = m_FreeConn;
    }

    connection_pool *connection_pool::GetInstance()
    {
        static connection_pool connPool;
        return &connPool;
    }

    connection_pool::connection_pool()
    {
        m_CurConn = 0;
        m_FreeConn = 0;
    }
    connection_pool::~connection_pool()
    {
        DestroyPool();
    }

    // int m_MaxConn;
    // int m_CurConn;
    // int m_FreeConn;
    // std::mutex m_mutex;
    // std::list<MYSQL *> connList;
    // std::counting_semaphore sem_reserve;

    connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
    {
        *SQL = connPool->GetConnection();
        connRAII = *SQL;
        poolRAII = connPool;
    }
    connectionRAII::~connectionRAII()
    {
        poolRAII->ReleaseConnection(connRAII);
    }
} // namespace tulun