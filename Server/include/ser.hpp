#ifndef HYPERTICKET_SERVER_HPP
#define HYPERTICKET_SERVER_HPP

#include <string>
#include <jsoncpp/json/json.h>

enum OP_TYPE {
    LOGIN = 1,  // 登陆
    REGISTER,   // 注册
    EXIT,       // 退出
    VIEW,       // 查看
    ORDER,      // 预定
    VIEW_MY,    // 查看我的预定
    CANCEL      // 取消预定
};

struct RequestMeta
{
    std::string client_id;
};

#endif // HYPERTICKET_SERVER_HPP