#include <arpa/inet.h>
#include "InetAddress.hpp"
#include<string.h>
#include<assert.h>

namespace shanchuan
{
    InetAddress::InetAddress() :addr_{0} { addr_.sin_family = AF_INET; } // 默认构造函数，初始化为0，并设置地址族为AF_INET
    InetAddress::InetAddress(const std::string &ip, const short port) :addr_{0} {
        addr_.sin_family = AF_INET; // 设置地址族为AF_INET
        SetIp(ip); 
        SetPort(port);
    }
    InetAddress::InetAddress(const struct sockaddr_in &addr) :addr_{addr} {}
    void InetAddress::SetIp(const std::string &ip) { inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr.s_addr); } // 将IP地址字符串转换为网络字节序的二进制形式，并存储在addr_.sin_addr.s_addr中
    void InetAddress::SetPort(const uint16_t port) { addr_.sin_port = htons(port);} // 将端口号转换为网络字节序，并存储在addr_.sin_port中
    std::string InetAddress::toIp() const {
        const int len = 32;
        char buff[len] = {};
        inet_ntop(AF_INET, &addr_.sin_addr, buff, len); // 将网络字节序的二进制IP地址转换为字符串形式，并存储在buff中
        return std::string(buff);
    }
    std::string InetAddress::toIpPort() const {
        const static int len = 128;
        char buff[len] = {};
        inet_ntop(AF_INET, &addr_.sin_addr, buff, len); // 将网络字节序的二进制IP地址转换为字符串形式，并存储在buff中
        int pos = strlen(buff);
        uint16_t port = ntohs(addr_.sin_port);
        assert(pos < len);
        sprintf(buff + pos, ":%u", port);  
        return std::string(buff);
    }
    uint16_t InetAddress::toPort() const { return ntohs(addr_.sin_port);}
    const struct sockaddr_in &InetAddress::getSockAddrInet() const { return addr_; }
    void InetAddress::setSockAddrInet(const struct sockaddr_in &addr) { addr_ = addr;}
    uint32_t InetAddress::ipNetEndian() const { return addr_.sin_addr.s_addr;}
    uint16_t InetAddress::portNetEndian() const { return addr_.sin_port;}
}
