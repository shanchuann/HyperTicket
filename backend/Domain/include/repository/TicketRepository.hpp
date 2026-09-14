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
        // 列表公共列（与 query() 的解析顺序一一对应）
        static constexpr const char *kListCols =
            "id, title, venue, total_seats, available_seats, event_date, status, "
            "cover_image, category, price, city, COALESCE(artist,'')";

        // 列出在售票务（status=1），用于客户端浏览。
        std::vector<Ticket> listOnSale(MYSQL *conn)
        {
            return query(conn, std::string("SELECT ") + kListCols +
                " FROM tickets WHERE status = 1 ORDER BY event_date");
        }

        // 条件搜索在售票务：keyword 模糊匹配 标题/场馆/艺人，city/category 精确。
        // 空串表示不过滤。全部走预编译参数，无注入风险。
        std::vector<Ticket> search(MYSQL *conn, const std::string &keyword,
                                   const std::string &city, const std::string &category)
        {
            std::string sql = std::string("SELECT ") + kListCols +
                " FROM tickets WHERE status = 1";
            if (!keyword.empty())
                sql += " AND (title LIKE ? OR venue LIKE ? OR artist LIKE ?)";
            if (!city.empty())
                sql += " AND city = ?";
            if (!category.empty())
                sql += " AND category = ?";
            sql += " ORDER BY event_date";

            std::vector<Ticket> out;
            MysqlStmt st(conn, sql);
            if (!st.ok()) return out;
            int idx = 0;
            std::string like = "%" + keyword + "%";
            if (!keyword.empty())
            {
                st.bindString(idx++, like);
                st.bindString(idx++, like);
                st.bindString(idx++, like);
            }
            if (!city.empty()) st.bindString(idx++, city);
            if (!category.empty()) st.bindString(idx++, category);
            if (!st.execute() || !st.bindResults(12)) return out;
            fetchRows(st, out);
            return out;
        }

        // 票品详情（含简介/须知，仅在售或已售罄票可见）。
        bool findDetail(MYSQL *conn, int64_t ticketId, Ticket &out)
        {
            MysqlStmt st(conn,
                "SELECT id, title, venue, total_seats, available_seats, event_date, status, "
                "       cover_image, category, price, city, COALESCE(artist,''), "
                "       COALESCE(description,''), COALESCE(notice,'') "
                "FROM tickets WHERE id = ? AND status != 0");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(14)) return false;
            if (!st.fetch()) return false;
            parseRow(st, out);
            out.description = st.getString(12);
            out.notice = st.getString(13);
            return true;
        }

        // 热门榜：按有效订单量（PENDING+CONFIRMED）排序的在售票 TOP N。
        std::vector<Ticket> hotTickets(MYSQL *conn, int limit)
        {
            std::string sql = std::string("SELECT ") + kListCols +
                ", (SELECT COUNT(*) FROM reservations r WHERE r.ticket_id = tickets.id "
                "   AND r.status IN ('PENDING','CONFIRMED')) AS hot "
                "FROM tickets WHERE status = 1 ORDER BY hot DESC, id DESC LIMIT ?";
            std::vector<Ticket> out;
            MysqlStmt st(conn, sql);
            if (!st.ok()) return out;
            st.bindInt(0, limit);
            if (!st.execute() || !st.bindResults(13)) return out;
            while (st.fetch())
            {
                Ticket t;
                parseRow(st, t);
                t.hotScore = static_cast<int>(st.getInt(12));
                out.push_back(t);
            }
            return out;
        }

        // 列出全部票务（admin 查看）。
        std::vector<Ticket> listAll(MYSQL *conn)
        {
            return query(conn, std::string("SELECT ") + kListCols + " FROM tickets");
        }

        // 锁定并读取单张票（FOR UPDATE，须在事务内）。未找到返回 false。
        bool lockForUpdate(MYSQL *conn, int64_t ticketId, Ticket &out)
        {
            MysqlStmt st(conn, "SELECT available_seats, status, price FROM tickets WHERE id = ? FOR UPDATE");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(3)) return false;
            if (!st.fetch()) return false;
            out.id = ticketId;
            out.availableSeats = static_cast<int>(st.getInt(0));
            out.status = static_cast<int>(st.getInt(1));
            out.price = static_cast<int>(st.getInt(2));
            return true;
        }

        // 库存增减（delta 可正可负）。
        bool adjustSeats(MYSQL *conn, int64_t ticketId, int delta)
        {
            // 库存扣减必须由数据库再次保证非负；即使上层缓存失效或发生
            // 重复请求，也不能将库存写成负数。InnoDB 行锁保证该条件检查与
            // 更新在同一条原子语句中完成。
            MysqlStmt st(conn,
                "UPDATE tickets SET available_seats = available_seats + ? "
                "WHERE id = ? AND (? >= 0 OR available_seats >= -?)");
            if (!st.ok()) return false;
            st.bindInt(0, delta);
            st.bindInt(1, ticketId);
            st.bindInt(2, delta);
            st.bindInt(3, delta);
            return st.execute() && st.affectedRows() == 1;
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

        // 新增票务（admin）。
        bool insert(MYSQL *conn, const std::string &title, const std::string &venue,
                    int totalSeats, const std::string &eventDate,
                    const std::string &coverImage = "",
                    const std::string &category = "concert",
                    int price = 0,
                    const std::string &city = "北京",
                    const std::string &artist = "",
                    const std::string &description = "",
                    const std::string &notice = "")
        {
            MysqlStmt st(conn,
                "INSERT INTO tickets (title, venue, total_seats, available_seats, event_date, status, "
                "cover_image, category, price, city, artist, description, notice) "
                "VALUES(?,?,?,?,?,1,?,?,?,?,?,?,?)");
            if (!st.ok()) return false;
            st.bindString(0, title);
            st.bindString(1, venue);
            st.bindInt(2, totalSeats);
            st.bindInt(3, totalSeats);
            st.bindString(4, eventDate);
            st.bindString(5, coverImage);
            st.bindString(6, category);
            st.bindInt(7, price);
            st.bindString(8, city);
            st.bindString(9, artist);
            st.bindString(10, description);
            st.bindString(11, notice);
            return st.execute();
        }

        // 获取刚插入的 ticket id（LAST_INSERT_ID）。
        int64_t lastInsertId(MYSQL *conn)
        {
            return static_cast<int64_t>(mysql_insert_id(conn));
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
        // 解析 kListCols 顺序的一行到 Ticket（不含 description/notice/hot）。
        static void parseRow(MysqlStmt &st, Ticket &t)
        {
            t.id = st.getInt(0);
            t.title = st.getString(1);
            t.venue = st.getString(2);
            t.totalSeats = static_cast<int>(st.getInt(3));
            t.availableSeats = static_cast<int>(st.getInt(4));
            t.eventDate = st.getString(5);
            t.status = static_cast<int>(st.getInt(6));
            t.coverImage = st.getString(7);
            t.category = st.getString(8);
            t.price = static_cast<int>(st.getInt(9));
            t.city = st.getString(10);
            t.artist = st.getString(11);
        }

        static void fetchRows(MysqlStmt &st, std::vector<Ticket> &out)
        {
            while (st.fetch())
            {
                Ticket t;
                parseRow(st, t);
                out.push_back(t);
            }
        }

        std::vector<Ticket> query(MYSQL *conn, const std::string &sql)
        {
            std::vector<Ticket> out;
            MysqlStmt st(conn, sql);
            if (!st.ok() || !st.execute() || !st.bindResults(12)) return out;
            fetchRows(st, out);
            return out;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_TICKET_REPOSITORY_HPP
