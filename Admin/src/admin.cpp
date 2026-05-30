#include "admin.hpp"
#include "../../Common/include/AppConfig.hpp"

bool AdminManager::ConnectDB() {
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, db_ip.c_str(), db_user.c_str(), 
                          db_passwd.c_str(), db_name.c_str(), 0, NULL, 0)) {
        cout << "连接数据库失败!" << endl;
        return false;
    }
    cout << "连接数据库成功!" << endl;
    return true;
}

void AdminManager::LoadConfig() {
    std::string error;
    hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json", &error);
    if (!error.empty()) {
        cout << "配置文件加载失败，使用默认配置: " << error << endl;
        return;
    }
    db_ip = cfg.db.host;
    db_user = cfg.db.user;
    db_name = cfg.db.name;
    db_passwd = cfg.db.password;
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
    string addr;
    int max;
    string use_date;
    
    cout << "\n添加门票信息:" << endl;
    cout << "场馆名称:";
    cin >> addr;
    
    cout << "总票数:";
    cin >> max;
    
    cout << "使用日期(YYYY-MM-DD):";
    cin >> use_date;
    
    string sql = "INSERT INTO tickets (title, venue, total_seats, available_seats, event_date, status) VALUES('";
    sql += addr + "', '" + addr + "', " + to_string(max) + ", " + to_string(max) + ", '";
    sql += use_date + "', 1)";
    
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "添加门票失败!" << endl;
        return;
    }
    cout << "添加门票成功!" << endl;
}

void AdminManager::ViewAllTickets() {
    string sql = "SELECT id, title, venue, total_seats, available_seats, event_date, status FROM tickets";
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "查询失败!" << endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(&mysql);
    if(res == NULL) {
        cout << "获取结果失败!" << endl;
        return;
    }

    cout << "\n当前所有门票信息:" << endl;
    cout << "+-------+---------------+---------------+--------+------+---------------+-------+" << endl;
    cout << "|门票ID |名称           |场馆           |总票数 |余票  |   使用日期    |状态   |" << endl;
    cout << "+-------+---------------+---------------+--------+------+---------------+-------+" << endl;

    MYSQL_ROW row;
    while((row = mysql_fetch_row(res))) {
        printf("|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|%s\t|\n", row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
        cout << "+-------+---------------+---------------+--------+------+---------------+-------+" << endl;
    }
    
    mysql_free_result(res);
}
void AdminManager::DeleteTicket() { //删除门票
    ViewAllTickets();
    int tk_id;
    cout << "请输入要取消的门票ID: ";
    cin >> tk_id;

    // 先检查门票是否存在
    string check_sql = "SELECT id FROM tickets WHERE id = " + to_string(tk_id);
    if(mysql_query(&mysql, check_sql.c_str()) != 0) {
        cout << "查询失败!" << endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(&mysql);
    if(res == NULL) {
        cout << "获取结果失败!" << endl;
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if(!row) {
        cout << "该门票不存在!" << endl;
        mysql_free_result(res);
        return;
    }

    mysql_free_result(res);

    // 删除门票
    string sql = "UPDATE tickets SET status = 0 WHERE id = " + to_string(tk_id);
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "删除门票失败!" << endl;
        return;
    }
    cout << "已将门票下架(软删除)!" << endl;
}

void AdminManager::ViewAllUsers() {
    string sql = "SELECT id, tel, username, password_hash, status FROM users";
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "查询失败!" << endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(&mysql);
    if(res == NULL) {
        cout << "获取结果失败!" << endl;
        return;
    }

    cout << "\n所有用户信息:" << endl;
    cout << "+--------+-------------+------------+--------------------------------+--------+" << endl;
    cout << "|用户ID  |   手机号    |   用户名   |          密码哈希              | 状态   |" << endl;
    cout << "+--------+-------------+------------+--------------------------------+--------+" << endl;

    MYSQL_ROW row;
    while((row = mysql_fetch_row(res))) {
        string status = (string(row[4]) == "1") ? "正常" : "黑名单";
        printf("|%-8s|%-13s|%-12s|%-32s|%-8s|\n", 
               row[0], row[1], row[2], row[3], status.c_str());
        cout << "+--------+-------------+------------+--------------------------------+--------+" << endl;
    }
    
    mysql_free_result(res);
}

void AdminManager::ViewBlacklist() {
    string sql = "SELECT id, tel, username FROM users WHERE status = 0";
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "查询失败!" << endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(&mysql);
    if(res == NULL) {
        cout << "获取结果失败!" << endl;
        return;
    }

    cout << "\n黑名单用户:" << endl;
    cout << "+--------+-------------+------------+" << endl;
    cout << "|用户ID  |   手机号    |   用户名   |" << endl;
    cout << "+--------+-------------+------------+" << endl;

    MYSQL_ROW row;
    while((row = mysql_fetch_row(res))) {
        printf("|%-8s|%-13s|%-12s|\n", row[0], row[1], row[2]);
        cout << "+--------+-------------+------------+" << endl;
    }
    
    mysql_free_result(res);
}

void AdminManager::AddToBlacklist() {
    string tel;
    cout << "请输入要加入黑名单的用户手机号: ";
    cin >> tel;

    // 先检查用户是否存在且状态为正常
    string check_sql = "SELECT status FROM users WHERE tel = '" + tel + "'";
    if(mysql_query(&mysql, check_sql.c_str()) != 0) {
        cout << "查询失败!" << endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(&mysql);
    if(res == NULL) {
        cout << "获取结果失败!" << endl;
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if(!row) {
        cout << "该用户不存在!" << endl;
        mysql_free_result(res);
        return;
    }

    if(string(row[0]) == "0") {
        cout << "该用户已经在黑名单中!" << endl;
        mysql_free_result(res);
        return;
    }

    mysql_free_result(res);

    // 更新用户状态为黑名单
    string sql = "UPDATE users SET status = 0 WHERE tel = '" + tel + "'";
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "加入黑名单失败!" << endl;
        return;
    }
    cout << "已将用户加入黑名单!" << endl;
}

void AdminManager::RemoveFromBlacklist() {
    string tel;
    cout << "请输入要移出黑名单的用户手机号: ";
    cin >> tel;

    // 先检查用户是否存在且在黑名单中
    string check_sql = "SELECT status FROM users WHERE tel = '" + tel + "'";
    if(mysql_query(&mysql, check_sql.c_str()) != 0) {
        cout << "查询失败!" << endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(&mysql);
    if(res == NULL) {
        cout << "获取结果失败!" << endl;
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if(!row) {
        cout << "该用户不存在!" << endl;
        mysql_free_result(res);
        return;
    }

    if(string(row[0]) == "1") {
        cout << "该用户不在黑名单中!" << endl;
        mysql_free_result(res);
        return;
    }

    mysql_free_result(res);

    // 更新用户状态为正常
    string sql = "UPDATE users SET status = 1 WHERE tel = '" + tel + "'";
    if(mysql_query(&mysql, sql.c_str()) != 0) {
        cout << "移出黑名单失败!" << endl;
        return;
    }
    cout << "已将用户移出黑名单!" << endl;
}

void AdminManager::Run() {
    int choice;
    while(running) {
        PrintMenu();
        cin >> choice;
        
        switch(choice) {
            case ADD_TICKET:
                AddTicket();
                break;
            case VIEW_TICKETS:
                ViewAllTickets();
                break;
            case VIEW_USERS:
                ViewAllUsers();
                break;
            case VIEW_BLACKLIST:
                ViewBlacklist();
                break;
            case ADD_TO_BLACKLIST:
                AddToBlacklist();
                break;
            case REMOVE_FROM_BLACKLIST:
                RemoveFromBlacklist();
                break;
            case CANCEL_TICKET:
                DeleteTicket();
                break;
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
    AdminManager admin;
    admin.LoadConfig();
    if(!admin.ConnectDB()) {
        return 1;
    }
    admin.Run();
    return 0;
}
