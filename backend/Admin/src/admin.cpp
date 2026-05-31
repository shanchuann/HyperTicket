#include "admin.hpp"
#include "../../Common/include/AppConfig.hpp"
#include "../../Domain/include/service/AdminService.hpp"
#include "../../ChronoLite/include/AsynLogging.hpp"

#include <cstdio>

// 管理端：保留控制台交互（菜单/输入/格式化输出），数据操作委托给 AdminService，
// 后者经 Repository 用预处理语句执行，与 ser 共享同一套防注入的数据访问。

bool AdminManager::ConnectDB() {
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, db_ip.c_str(), db_user.c_str(),
                          db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0)) {
        cout << "连接数据库失败!" << endl;
        LOG_ERROR << "DB connect failed: " << mysql_error(&mysql);
        return false;
    }
    cout << "连接数据库成功!" << endl;
    LOG_INFO << "DB connected " << db_ip << "/" << db_name;
    return true;
}

bool AdminManager::LoadConfig() {
    std::string error;
    hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json", &error);
    if (!error.empty()) {
        cout << "配置文件加载失败: " << error << endl;
        return false;
    }
    db_ip = cfg.db.host;
    db_port = cfg.db.port;
    db_user = cfg.db.user;
    db_name = cfg.db.name;
    db_passwd = cfg.db.password;
    return true;
}

void AdminManager::PrintMenu() {
    cout << "\n+--------------------------------+" << endl;
    cout << "| 管理员操作界面                 |" << endl;
    cout << "+--------------------------------+" << endl;
    cout << "| 1.添加门票                     |" << endl;
    cout << "| 2.查看所有门票                 |" << endl;
    cout << "| 3.查看所有用户                 |" << endl;
    cout << "| 4.查看黑名单用户               |" << endl;
    cout << "| 5.将用户加入黑名单             |" << endl;
    cout << "| 6.将用户移出黑名单             |" << endl;
    cout << "| 7.删除门票信息                 |" << endl;
    cout << "| 8.退出                         |" << endl;
    cout << "+--------------------------------+" << endl;
    cout << "请输入操作编号: ";
}

void AdminManager::AddTicket() {
    string addr, use_date;
    int max;
    cout << "\n添加门票信息:" << endl;
    cout << "场馆名称:";
    cin >> addr;
    cout << "总票数:";
    cin >> max;
    cout << "使用日期(YYYY-MM-DD):";
    cin >> use_date;

    bool ok = svc_->addTicket(addr, max, use_date);
    cout << (ok ? "添加门票成功!" : "添加门票失败!") << endl;
    LOG_INFO << "addTicket venue=" << addr << " seats=" << max << " date=" << use_date << " ok=" << ok;
}

void AdminManager::ViewAllTickets() {
    auto tickets = svc_->listAllTickets();
    cout << "\n当前所有门票信息:" << endl;
    cout << "+-------+---------------+---------------+--------+------+---------------+-------+" << endl;
    cout << "|门票ID |名称           |场馆           |总票数 |余票  |   使用日期    |状态   |" << endl;
    cout << "+-------+---------------+---------------+--------+------+---------------+-------+" << endl;
    for (const auto &t : tickets) {
        printf("|%lld\t|%s\t|%s\t|%d\t|%d\t|%s\t|%d\t|\n",
               (long long)t.id, t.title.c_str(), t.venue.c_str(),
               t.totalSeats, t.availableSeats, t.eventDate.c_str(), t.status);
        cout << "+-------+---------------+---------------+--------+------+---------------+-------+" << endl;
    }
}

void AdminManager::DeleteTicket() {
    ViewAllTickets();
    int tk_id;
    cout << "请输入要取消的门票ID: ";
    cin >> tk_id;

    if (!svc_->deleteTicket(tk_id)) {
        cout << "该门票不存在或删除失败!" << endl;
        LOG_WARN << "deleteTicket failed id=" << tk_id;
        return;
    }
    cout << "已将门票下架(软删除)!" << endl;
    LOG_INFO << "deleteTicket ok id=" << tk_id;
}

void AdminManager::ViewAllUsers() {
    auto users = svc_->listAllUsers();
    cout << "\n所有用户信息:" << endl;
    cout << "+--------+-------------+------------+--------+" << endl;
    cout << "|用户ID  |   手机号    |   用户名   | 状态   |" << endl;
    cout << "+--------+-------------+------------+--------+" << endl;
    for (const auto &u : users) {
        string st = (u.status == 1) ? "正常" : "黑名单";
        printf("|%-8lld|%-13s|%-12s|%-8s|\n",
               (long long)u.id, u.tel.c_str(), u.username.c_str(), st.c_str());
        cout << "+--------+-------------+------------+--------+" << endl;
    }
}

void AdminManager::ViewBlacklist() {
    auto users = svc_->listBlacklist();
    cout << "\n黑名单用户:" << endl;
    cout << "+--------+-------------+------------+" << endl;
    cout << "|用户ID  |   手机号    |   用户名   |" << endl;
    cout << "+--------+-------------+------------+" << endl;
    for (const auto &u : users) {
        printf("|%-8lld|%-13s|%-12s|\n", (long long)u.id, u.tel.c_str(), u.username.c_str());
        cout << "+--------+-------------+------------+" << endl;
    }
}

namespace {
    void reportBlacklist(hyperticket::BlacklistResult r, const char *okMsg, const char *stateMsg) {
        using hyperticket::BlacklistResult;
        switch (r) {
            case BlacklistResult::Ok:             cout << okMsg << endl; break;
            case BlacklistResult::UserNotFound:   cout << "该用户不存在!" << endl; break;
            case BlacklistResult::AlreadyInState: cout << stateMsg << endl; break;
            default:                              cout << "操作失败!" << endl; break;
        }
    }
}

void AdminManager::AddToBlacklist() {
    string tel;
    cout << "请输入要加入黑名单的用户手机号: ";
    cin >> tel;
    auto r = svc_->addToBlacklist(tel);
    reportBlacklist(r, "已将用户加入黑名单!", "该用户已经在黑名单中!");
    LOG_INFO << "addToBlacklist tel=" << tel << " result=" << static_cast<int>(r);
}

void AdminManager::RemoveFromBlacklist() {
    string tel;
    cout << "请输入要移出黑名单的用户手机号: ";
    cin >> tel;
    auto r = svc_->removeFromBlacklist(tel);
    reportBlacklist(r, "已将用户移出黑名单!", "该用户不在黑名单中!");
    LOG_INFO << "removeFromBlacklist tel=" << tel << " result=" << static_cast<int>(r);
}

void AdminManager::Run() {
    // 登录验证
    string username, password;
    cout << "管理员用户名: ";
    cin >> username;
    cout << "密码: ";
    cin >> password;

    hyperticket::AdminService svc(&mysql);
    if (!svc.login(username, password)) {
        cout << "用户名或密码错误!" << endl;
        LOG_WARN << "admin login failed username=" << username;
        return;
    }
    cout << "登录成功! 欢迎, " << svc.adminUsername() << " (" << svc.adminRole() << ")" << endl;
    LOG_INFO << "admin login ok username=" << username << " role=" << svc.adminRole();

    // 检测默认密码，强制修改
    if (svc.isDefaultPassword()) {
        cout << "\n*** 安全警告：您正在使用默认密码，必须立即修改！***" << endl;
        string newPwd, confirmPwd;
        do {
            cout << "新密码（6-16位，须含数字、小写、大写字母）: ";
            cin >> newPwd;
            cout << "确认新密码: ";
            cin >> confirmPwd;
            if (newPwd != confirmPwd) { cout << "两次密码不一致，请重试。" << endl; continue; }
            if (!svc.changePassword(username, newPwd)) { cout << "密码不符合要求，请重试。" << endl; continue; }
            break;
        } while (true);
        cout << "密码已修改！" << endl;
    }

    // 登录后创建服务实例，后续操作复用
    svc_ = new hyperticket::AdminService(&mysql);

    int choice;
    while(running) {
        PrintMenu();
        cin >> choice;
        switch(choice) {
            case ADD_TICKET:            AddTicket(); break;
            case VIEW_TICKETS:          ViewAllTickets(); break;
            case VIEW_USERS:            ViewAllUsers(); break;
            case VIEW_BLACKLIST:        ViewBlacklist(); break;
            case ADD_TO_BLACKLIST:      AddToBlacklist(); break;
            case REMOVE_FROM_BLACKLIST: RemoveFromBlacklist(); break;
            case CANCEL_TICKET:         DeleteTicket(); break;
            case EXIT:
                running = false;
                cout << "退出管理系统!" << endl;
                break;
            default:
                cout << "无效的选择!" << endl;
                break;
        }
    }
}

int main() {
    logsys::AsynLogging asyncLogger("logs/hyperticket.admin", 16 * 1024 * 1024, 3);
    logsys::Logger::SetOuput([&asyncLogger](const std::string &msg) { asyncLogger.append(msg); });
    logsys::Logger::SetFlush([&asyncLogger]() { asyncLogger.flush(); });
    logsys::Logger::SetLogLevel(logsys::LOG_LEVEL::INFO);
    asyncLogger.start();

    AdminManager admin;
    if(!admin.LoadConfig()) return 1;
    if(!admin.ConnectDB()) return 1;
    admin.Run();
    return 0;
}
