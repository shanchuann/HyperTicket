#ifndef HYPERTICKET_PROTOCOL_HPP
#define HYPERTICKET_PROTOCOL_HPP

#include <string>
#include <jsoncpp/json/json.h>

namespace hyperticket
{
    // 客户端与服务端共享的请求类型枚举（原先在 ser.hpp 与 client.hpp 各定义一份）。
    enum RequestType
    {
        LOGIN = 1,    // 登录
        REGISTER = 2, // 注册
        EXIT = 3,     // 退出
        VIEW = 4,     // 查看所有票务
        ORDER = 5,    // 预定
        VIEW_MY = 6,  // 查看本人预定
        CANCEL = 7,   // 取消预定

        // 管理员 API（需携带 admin_token）
        ADMIN_LOGIN = 8,          // 管理员登录
        ADMIN_LIST_TICKETS = 9,   // 查看所有票务（含下架）
        ADMIN_ADD_TICKET = 10,    // 新增票务
        ADMIN_DELETE_TICKET = 11, // 下架票务
        ADMIN_LIST_USERS = 12,    // 查看所有用户
        ADMIN_STATS = 13,             // 统计数据
        ADMIN_BLACKLIST = 14,         // 黑名单管理
        ADMIN_CHANGE_PASSWORD = 15,   // 修改管理员密码

        // 用户扩展 API
        DELETE_ORDER = 16,   // 删除已取消/已过期的预定记录
        VIEW_SEATS   = 17,   // 查看票务座位图
        VERIFY_ORDER = 18,   // 根据订单号验证票务状态（无需认证）

        // v2 大麦网式功能扩展
        TICKET_DETAIL  = 19, // 票品详情（简介/购票须知/艺人，无需认证）
        PAY_ORDER      = 20, // 发起支付（v3 起创建支付流水并提交模拟网关，异步结算）
        FAVORITE       = 21, // 收藏/取消收藏（action: add|remove）
        VIEW_FAVORITES = 22, // 我的收藏列表
        HOT_TICKETS    = 23, // 热门榜（按有效订单量 TOP N，无需认证）

        // v3 支付模块
        PAY_QUERY      = 24, // 查询支付结果（前端发起支付后轮询）
        ORDER_QUERY    = 25, // 查询异步下单请求状态
        VERIFICATION_REQUEST = 26,
        VERIFICATION_VERIFY  = 27,
        PASSWORD_RESET_REQUEST = 28,
        PASSWORD_RESET_VERIFY  = 29,
        PASSWORD_RESET_CONFIRM = 30,
        ACCOUNT_SECURITY_STATUS = 31,
        CONTACT_VERIFICATION_REQUEST = 32,
        CONTACT_VERIFICATION_CONFIRM = 33,
        CATALOG_HOME = 34,
        EVENT_DETAIL = 35,
        PROFILE_GET = 36,
        PROFILE_UPDATE = 37,
        ATTENDEE_LIST = 38,
        ATTENDEE_MUTATE = 39,
        BROWSING_HISTORY = 40,
        SALE_REMINDER_LIST = 41,
        SALE_REMINDER_MUTATE = 42,
        EVENT_FAVORITE = 43,
        ADMIN_CATALOG = 44,
        ADMIN_CATALOG_MUTATE = 45,
        ADMIN_REMINDER_LIST = 46,
    };

    // 协议字段名常量，避免魔法字符串散落各处。
    namespace field
    {
        constexpr const char *kType = "type";
        constexpr const char *kStatus = "status";
        constexpr const char *kReason = "reason";
        constexpr const char *kToken = "token";
        constexpr const char *kUserTel = "usertel";
        constexpr const char *kPassword = "passward"; // 保留历史拼写以兼容现有客户端
        constexpr const char *kUserName = "username";
        constexpr const char *kIndex = "index";
        constexpr const char *kNum = "num";
        constexpr const char *kArr = "arr";

        // 管理员专用字段
        constexpr const char *kAdminToken = "admin_token";
        constexpr const char *kTitle = "title";
        constexpr const char *kVenue = "venue";
        constexpr const char *kEventDate = "event_date";
        constexpr const char *kTotalSeats = "total_seats";
        constexpr const char *kPrice = "price";
        constexpr const char *kTicketId = "ticket_id";
        constexpr const char *kAction = "action"; // "add" | "remove"
        constexpr const char *kTel = "tel";

        // v2 扩展字段
        constexpr const char *kCity = "city";
        constexpr const char *kKeyword = "keyword";
        constexpr const char *kCategory = "category";
        constexpr const char *kQuantity = "quantity";
        constexpr const char *kDescription = "description";
        constexpr const char *kNotice = "notice";
        constexpr const char *kArtist = "artist";

        // v3 支付模块字段
        constexpr const char *kMethod = "method";               // 兼容字段；新客户端使用 provider
        constexpr const char *kProvider = "provider";
        constexpr const char *kProviderTransactionId = "provider_transaction_id";
        constexpr const char *kIdempotencyKey = "idempotency_key";
        constexpr const char *kPaymentNo = "payment_no";        // 支付单号
        constexpr const char *kPaymentStatus = "payment_status";
        constexpr const char *kAmount = "amount";               // 兼容：元
        constexpr const char *kAmountMinor = "amount_minor";    // 最小货币单位，CNY 时为分
        constexpr const char *kCurrency = "currency";
        constexpr const char *kEmail = "email";
        constexpr const char *kChannel = "channel";
        constexpr const char *kPurpose = "purpose";
        constexpr const char *kChallengeId = "challenge_id";
        constexpr const char *kCode = "code";
        constexpr const char *kVerificationToken = "verification_token";
        constexpr const char *kResetToken = "reset_token";
    }

    // status 字段取值
    namespace status
    {
        constexpr const char *kOk = "OK";
        constexpr const char *kErr = "ERR";
    }

    // 统一的响应工厂
    inline Json::Value makeError(const std::string &reason)
    {
        Json::Value res;
        res[field::kStatus] = status::kErr;
        res[field::kReason] = reason;
        return res;
    }

    inline Json::Value makeOk()
    {
        Json::Value res;
        res[field::kStatus] = status::kOk;
        return res;
    }

    // 序列化为一行 JSON（按 '\n' 分隔的协议）。
    inline std::string toJsonLine(const Json::Value &value)
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string out = Json::writeString(builder, value);
        out.push_back('\n');
        return out;
    }
} // namespace hyperticket
#endif // HYPERTICKET_PROTOCOL_HPP
