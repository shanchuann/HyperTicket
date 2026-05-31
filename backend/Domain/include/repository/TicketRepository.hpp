#ifndef HYPERTICKET_TICKET_REPOSITORY_HPP
#define HYPERTICKET_TICKET_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // tickets 表的数据访问。注意：带 FOR UPDATE 的行锁方法必须在调用方已开启的
    // 事务（BEGIN）内使用，事务边界由 Service 层管理。
    class TicketRepository
    {
    public:
        // 列出在售票务（status=1），用于客户端浏览。
        std::vector<Ticket> listOnSale(MYSQL *conn)
        {
            return query(conn,
                "SELECT id, title, venue, total_seats, available_seats, event_date, status "
                "FROM tickets WHERE status = 1");
        }

        // 列出全部票务（admin 查看）。
        std::vector<Ticket> listAll(MYSQL *conn)
        {
            return query(conn,
                "SELECT id, title, venue, total_seats, available_seats, event_date, status FROM tickets");
        }

        // 锁定并读取单张票（FOR UPDATE，须在事务内）。未找到返回 false。
        bool lockForUpdate(MYSQL *conn, int64_t ticketId, Ticket &out)
        {
            MysqlStmt st(conn, "SELECT available_seats, status FROM tickets WHERE id = ? FOR UPDATE");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(2)) return false;
            if (!st.fetch()) return false;
            out.id = ticketId;
            out.availableSeats = static_cast<int>(st.getInt(0));
            out.status = static_cast<int>(st.getInt(1));
            return true;
        }

        // 库存增减（delta 可正可负）。
        bool adjustSeats(MYSQL *conn, int64_t ticketId, int delta)
        {
            MysqlStmt st(conn, "UPDATE tickets SET available_seats = available_seats + ? WHERE id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, delta);
            st.bindInt(1, ticketId);
            return st.execute();
        }

        // 是否存在指定票。
        bool exists(MYSQL *conn, int64_t ticketId)
        {
            MysqlStmt st(conn, "SELECT id FROM tickets WHERE id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(1)) return false;
            return st.fetch();
        }

        // 新增票务（admin）。title 与 venue 取同值以兼容现有 admin 行为。
        bool insert(MYSQL *conn, const std::string &title, const std::string &venue,
                    int totalSeats, const std::string &eventDate)
        {
            MysqlStmt st(conn,
                "INSERT INTO tickets (title, venue, total_seats, available_seats, event_date, status) "
                "VALUES(?,?,?,?,?,1)");
            if (!st.ok()) return false;
            st.bindString(0, title);
            st.bindString(1, venue);
            st.bindInt(2, totalSeats);
            st.bindInt(3, totalSeats);
            st.bindString(4, eventDate);
            return st.execute();
        }

        // 下架票务（软删除 status=0）。
        bool setOffline(MYSQL *conn, int64_t ticketId)
        {
            MysqlStmt st(conn, "UPDATE tickets SET status = 0 WHERE id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            return st.execute();
        }

        // 批量将过期票下架（定时任务）。
        bool offlineExpired(MYSQL *conn)
        {
            return mysql_query(conn,
                "UPDATE tickets SET status = 0 WHERE event_date < CURDATE() AND status != 0") == 0;
        }

    private:
        std::vector<Ticket> query(MYSQL *conn, const std::string &sql)
        {
            std::vector<Ticket> out;
            MysqlStmt st(conn, sql);
            if (!st.ok() || !st.execute() || !st.bindResults(7)) return out;
            while (st.fetch())
            {
                Ticket t;
                t.id = st.getInt(0);
                t.title = st.getString(1);
                t.venue = st.getString(2);
                t.totalSeats = static_cast<int>(st.getInt(3));
                t.availableSeats = static_cast<int>(st.getInt(4));
                t.eventDate = st.getString(5);
                t.status = static_cast<int>(st.getInt(6));
                out.push_back(t);
            }
            return out;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_TICKET_REPOSITORY_HPP
