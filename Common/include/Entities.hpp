#ifndef HYPERTICKET_ENTITIES_HPP
#define HYPERTICKET_ENTITIES_HPP

#include <cstdint>
#include <string>

namespace hyperticket
{
    // 数据访问层（Repository）返回的领域实体，对应 db/init.sql 中的表。
    // 用纯数据结构（POD）解耦：上层 Service 不直接接触 MYSQL_ROW。

    struct User
    {
        int64_t id = 0;
        std::string tel;
        std::string username;
        std::string passwordHash;
        int status = 1; // 1 正常 / 0 黑名单
    };

    struct Ticket
    {
        int64_t id = 0;
        std::string title;
        std::string venue;
        int totalSeats = 0;
        int availableSeats = 0;
        std::string eventDate;
        int status = 1; // 1 在售 / 0 下架
    };

    struct Reservation
    {
        int64_t id = 0;
        int64_t userId = 0;
        int64_t ticketId = 0;
        int quantity = 1;
        std::string status; // PENDING/CONFIRMED/CANCELLED/EXPIRED
        // 联表查询时附带的票务信息（viewMyTickets 用）
        std::string ticketTitle;
        std::string ticketVenue;
        std::string eventDate;
    };
} // namespace hyperticket
#endif // HYPERTICKET_ENTITIES_HPP
