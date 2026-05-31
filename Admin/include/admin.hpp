#ifdef _WIN32
// Windows 特定的代码
#include <windows.h>
#include<iostream>
#include<string>
#include <mysql.h>
#elif __linux__
// Linux 特定的代码
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#else
#error "Unsupported operating system"
#endif
#include "../../ChronoLite/include/Logger.hpp"
#include "../../Domain/include/service/AdminService.hpp"
using namespace std;

enum ADMIN_OP {
    ADD_TICKET = 1,    // 添加门票
    VIEW_TICKETS = 2,  // 查看所有门票
    VIEW_USERS = 3,    // 查看所有用户
    VIEW_BLACKLIST = 4,// 查看黑名单
    ADD_TO_BLACKLIST = 5,  // 加入黑名单
    REMOVE_FROM_BLACKLIST = 6, // 移出黑名单
    CANCEL_TICKET = 7, // 取消门票
    EXIT = 8          // 退出
};

class AdminManager {
public:
    AdminManager() {
        // DB settings are populated by LoadConfig() from config.json / .env.
        running = true;
    }
    ~AdminManager() {
        mysql_close(&mysql);
    }

    bool ConnectDB();
    bool LoadConfig();
    void Run();
    void PrintMenu();
    
    // 门票管理
    void AddTicket();
    void ViewAllTickets();
    void DeleteTicket();
    
    // 用户管理(后续实现)
    void ViewAllUsers();
    
    // 增加黑名单相关操作的函数声明
    void ViewBlacklist();
    void AddToBlacklist();
    void RemoveFromBlacklist();

private:
    MYSQL mysql;
    string db_ip;
    int db_port = 3306;
    string db_user;
    string db_name;
    string db_passwd;
    bool running;
    hyperticket::AdminService *svc_ = nullptr;  // 登录后创建，操作复用
};