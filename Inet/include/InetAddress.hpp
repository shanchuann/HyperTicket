#ifndef INETADDRESS_HPP
#define INETADDRESS_HPP

#include <netinet/in.h>
#include <string>

namespace shanchuan
{
    // 地址类，封装了sockaddr_in结构体
    class InetAddress
    {
    private:
        struct sockaddr_in addr_; // 结构体成员变量，存储IP地址和端口号
    public:
        InetAddress();
        InetAddress(const std::string &ip, const short port);   // 接受IP地址和端口号
        InetAddress(const struct sockaddr_in &addr);            // 接受sockaddr_in结构体
        void SetIp(const std::string &ip);
        void SetPort(const uint16_t port);
        std::string toIp() const;                               // "192.168.1.1"
        std::string toIpPort() const;                           // "192.168.1.1:1234"
        uint16_t toPort() const;                                // 1234
        const struct sockaddr_in &getSockAddrInet() const;      // 获取sockaddr_in结构体
        void setSockAddrInet(const struct sockaddr_in &addr);   // 直接设置sockaddr_in结构体
        uint32_t ipNetEndian() const;
        uint16_t portNetEndian() const;
    };
} // namespace shanchuan
#endif // INETADDRESS_HPP