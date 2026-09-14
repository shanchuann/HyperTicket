#ifndef HYPERTICKET_TXN_HPP
#define HYPERTICKET_TXN_HPP

#include <mysql/mysql.h>

namespace hyperticket
{
    // 简单的事务 RAII：构造 BEGIN，析构时若未提交则 ROLLBACK，避免遗漏回滚。
    // （原先是 TicketServiceTxn.cpp 的内部类，支付模块也需要，抽出共享。）
    class Txn
    {
    public:
        explicit Txn(MYSQL *conn) : conn_(conn)
        {
            began_ = (mysql_query(conn_, "BEGIN") == 0);
        }
        ~Txn()
        {
            if (began_ && !done_) mysql_query(conn_, "ROLLBACK");
        }
        bool ok() const { return began_; }
        bool commit()
        {
            if (mysql_query(conn_, "COMMIT") != 0) return false;
            done_ = true;
            return true;
        }
        void rollback()
        {
            mysql_query(conn_, "ROLLBACK");
            done_ = true;
        }

    private:
        MYSQL *conn_;
        bool began_ = false;
        bool done_ = false;
    };
} // namespace hyperticket
#endif // HYPERTICKET_TXN_HPP
