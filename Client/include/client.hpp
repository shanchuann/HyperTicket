#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

// 平台检测
#ifdef _WIN32
    // Windows 平台
#include <windows.h>
#include <winsock2.h>
#include <io.h>
#include <process.h>
#include <mysql.h>
#pragma comment(lib, "ws2_32.lib") // 链接 Winsock 库
#elif __linux__
    // Linux 平台
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>
#else
#error "Unsupported operating system"
#endif

// 公共头文件
#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>

// 第三方库头文件
#ifdef _WIN32
#include <json/json.h>    // Windows 上的 jsoncpp
#elif __linux__
#include <jsoncpp/json/json.h> // Linux 上的 jsoncpp
#endif
using namespace std;
const int OFFSET = 3;

// 请求类型枚举与服务端共享，统一定义在 Common/Protocol.hpp，消除重复。
#include "../../Common/include/Protocol.hpp"
#include "../../ChronoLite/include/Logger.hpp"
using hyperticket::LOGIN;
using hyperticket::REGISTER;
using hyperticket::EXIT;
using hyperticket::VIEW;
using hyperticket::ORDER;
using hyperticket::VIEW_MY;
using hyperticket::CANCEL;

class socket_client{
public:
    socket_client(){
        sockfd = -1;
        ips = "127.0.0.1";
        port = 7000;
        dl_flg = false;
        user_op = 0;
        running = true;
    }
    socket_client(string ips, short port){
        this->ips = ips;
        this->port = port;
        sockfd = -1;
        dl_flg = false;
        user_op = 0;
        running = true;
    }
    void print_info();
    ~socket_client(){
        close(sockfd);
    }
    void LoadConfig();
    bool Connect_server();
    void Run();
    void login();
    void register_();
    void exit_();
    void view();
    void order();
    void view_my();
    void cancel();
private:
    string ips;
    short port;
    int sockfd;
    bool dl_flg;
    bool running;
    string username;
    string usertel;
    string token;       // 登录后服务端签发的会话令牌，用于后续鉴权

    int user_op;

    Json::Value m_val;

    std::string recvBuf_;  // 接收缓冲区：保存多消息时剩余数据

    bool send_json(const Json::Value &val);
    bool recv_json(Json::Value &out);
};
#endif // SOCKET_CLIENT_H