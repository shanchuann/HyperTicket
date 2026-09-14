#ifndef HYPERTICKET_FAVORITE_REPOSITORY_HPP
#define HYPERTICKET_FAVORITE_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    // favorites 表（用户收藏/想看）的数据访问。
    class FavoriteRepository
    {
    public:
        // 添加收藏（重复收藏静默成功，UNIQUE KEY 兜底）。
        bool add(MYSQL *conn, int64_t userId, int64_t ticketId)
        {
            MysqlStmt st(conn,
                "INSERT IGNORE INTO favorites (user_id, ticket_id) VALUES(?,?)");
            if (!st.ok()) return false;
            st.bindInt(0, userId);
            st.bindInt(1, ticketId);
            return st.execute();
        }

        // 取消收藏。
        bool remove(MYSQL *conn, int64_t userId, int64_t ticketId)
        {
            MysqlStmt st(conn,
                "DELETE FROM favorites WHERE user_id = ? AND ticket_id = ?");
            if (!st.ok()) return false;
            st.bindInt(0, userId);
            st.bindInt(1, ticketId);
            return st.execute();
        }

        // 我的收藏（联表带票务信息，含已下架票以便提示用户）。
        std::vector<Ticket> listByUser(MYSQL *conn, int64_t userId)
        {
            std::vector<Ticket> out;
            MysqlStmt st(conn,
                "SELECT t.id, t.title, t.venue, t.total_seats, t.available_seats, "
                "       t.event_date, t.status, t.cover_image, t.category, t.price, "
                "       t.city, COALESCE(t.artist,'') "
                "FROM favorites f JOIN tickets t ON f.ticket_id = t.id "
                "WHERE f.user_id = ? ORDER BY f.id DESC");
            if (!st.ok()) return out;
            st.bindInt(0, userId);
            if (!st.execute() || !st.bindResults(12)) return out;
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
                t.coverImage = st.getString(7);
                t.category = st.getString(8);
                t.price = static_cast<int>(st.getInt(9));
                t.city = st.getString(10);
                t.artist = st.getString(11);
                out.push_back(t);
            }
            return out;
        }

        // 用户收藏的 ticket_id 集合（列表页标注心形用）。
        std::vector<int64_t> listIds(MYSQL *conn, int64_t userId)
        {
            std::vector<int64_t> out;
            MysqlStmt st(conn, "SELECT ticket_id FROM favorites WHERE user_id = ?");
            if (!st.ok()) return out;
            st.bindInt(0, userId);
            if (!st.execute() || !st.bindResults(1)) return out;
            while (st.fetch()) out.push_back(st.getInt(0));
            return out;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_FAVORITE_REPOSITORY_HPP
