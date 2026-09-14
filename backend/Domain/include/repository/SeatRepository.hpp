#ifndef HYPERTICKET_SEAT_REPOSITORY_HPP
#define HYPERTICKET_SEAT_REPOSITORY_HPP

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <cmath>

#include "../../../Common/include/Entities.hpp"
#include "../MysqlStmt.hpp"

namespace hyperticket
{
    class SeatRepository
    {
    public:
        // 按票务 id 批量生成座位（单条 batch INSERT，比循环单条高效且无连接状态问题）。
        bool generate(MYSQL *conn, int64_t ticketId, int totalSeats, int basePrice)
        {
            if (totalSeats <= 0) return true;
            const int cols = (totalSeats <= 50) ? 10 : (totalSeats <= 200 ? 15 : 20);
            const int rows = (totalSeats + cols - 1) / cols;
            const int vipRows      = std::max(1, static_cast<int>(rows * 0.15));
            const int standardRows = std::max(1, static_cast<int>(rows * 0.35));

            // 构建 batch INSERT SQL
            std::string sql =
                "INSERT IGNORE INTO seats "
                "(ticket_id, seat_label, row_label, col_num, tier, price) VALUES ";

            int seatCount = 0;
            bool first = true;
            for (int r = 0; r < rows && seatCount < totalSeats; ++r)
            {
                std::string rowLabel = rowName(r);
                std::string tier;
                int price;
                if (r < vipRows) {
                    tier = "VIP";
                    price = static_cast<int>(basePrice * 1.5);
                } else if (r < vipRows + standardRows) {
                    tier = "Standard";
                    price = basePrice;
                } else {
                    tier = "Economy";
                    price = static_cast<int>(basePrice * 0.7);
                }

                for (int c = 1; c <= cols && seatCount < totalSeats; ++c, ++seatCount)
                {
                    std::string label = rowLabel + std::to_string(c);
                    if (!first) sql += ',';
                    sql += '(';
                    sql += std::to_string(ticketId) + ',';
                    sql += "'" + label + "',";
                    sql += "'" + rowLabel + "',";
                    sql += std::to_string(c) + ',';
                    sql += "'" + tier + "',";
                    sql += std::to_string(price) + ')';
                    first = false;
                }
            }

            return mysql_query(conn, sql.c_str()) == 0;
        }

        // 查询票务的所有座位。
        std::vector<Seat> listByTicket(MYSQL *conn, int64_t ticketId)
        {
            std::vector<Seat> out;
            MysqlStmt st(conn,
                "SELECT id, seat_label, row_label, col_num, tier, price, status "
                "FROM seats WHERE ticket_id = ? ORDER BY row_label, col_num");
            if (!st.ok()) return out;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(7)) return out;
            while (st.fetch())
            {
                Seat s;
                s.id = st.getInt(0);
                s.ticketId = ticketId;
                s.seatLabel = st.getString(1);
                s.rowLabel = st.getString(2);
                s.colNum = static_cast<int>(st.getInt(3));
                s.tier = st.getString(4);
                s.price = static_cast<int>(st.getInt(5));
                s.status = st.getString(6);
                out.push_back(s);
            }
            return out;
        }

        // 是否已为该票务生成了座位。
        bool hasSeats(MYSQL *conn, int64_t ticketId)
        {
            MysqlStmt st(conn, "SELECT 1 FROM seats WHERE ticket_id = ? LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(1)) return false;
            return st.fetch();
        }

        // 锁定座位并标记为 SOLD（必须在事务内，FOR UPDATE）。
        // 返回 false 表示不存在或已被占用。
        bool lockAndSell(MYSQL *conn, int64_t seatId, int64_t ticketId, int64_t reservationId)
        {
            // 先 FOR UPDATE 检查 AVAILABLE
            MysqlStmt chk(conn,
                "SELECT status FROM seats WHERE id = ? AND ticket_id = ? FOR UPDATE");
            if (!chk.ok()) return false;
            chk.bindInt(0, seatId);
            chk.bindInt(1, ticketId);
            if (!chk.execute() || !chk.bindResults(1)) return false;
            if (!chk.fetch()) return false;
            if (chk.getString(0) != "AVAILABLE") return false;

            MysqlStmt upd(conn,
                "UPDATE seats SET status='SOLD', reservation_id=? WHERE id=?");
            if (!upd.ok()) return false;
            upd.bindInt(0, reservationId);
            upd.bindInt(1, seatId);
            return upd.execute();
        }

        // 票务是否有剩余可选座位。
        bool hasAvailable(MYSQL *conn, int64_t ticketId)
        {
            MysqlStmt st(conn,
                "SELECT 1 FROM seats WHERE ticket_id=? AND status='AVAILABLE' LIMIT 1");
            if (!st.ok()) return false;
            st.bindInt(0, ticketId);
            if (!st.execute() || !st.bindResults(1)) return false;
            return st.fetch();
        }

    private:
        // 把行号(0-based)转为字母标签: 0→A, 25→Z, 26→AA, ...
        std::string rowName(int n)
        {
            std::string s;
            do {
                s = static_cast<char>('A' + n % 26) + s;
                n = n / 26 - 1;
            } while (n >= 0);
            return s;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_SEAT_REPOSITORY_HPP
