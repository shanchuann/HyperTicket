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
