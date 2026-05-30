#ifndef SOCKETSOPS_HPP
#define SOCKETSOPS_HPP
#include <arpa/inet.h>

namespace shanchuan
{
    namespace Sockets
    {
        void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr);   // 将ip地址和端口号转换为sockaddr_in结构体
        void toIpPort(char *buff, size_t len, const struct sockaddr_in &addr);      // 将sockaddr_in结构体转换为 ip:port
        void toIp(char *buff, size_t len, const struct sockaddr_in &addr);          // 将sockaddr_in结构体转换为ip地址
        void setNonBlock(int sockfd);                                               // 将sockfd设置为非阻塞模式
        void close(int sockfd);
        void bindOrDie(int sockfd, const struct sockaddr_in &addr);                 // 绑定socket到地址
        void listenOrDie(int sockfd);
        int accept(int sockfd, struct sockaddr_in *addr);                           // 接受连接请求
        void shutdownWrite(int sockfd);                                             // 关闭写入端
        int createNonblockingOrDie();                                               // 非阻塞socket
        int createBlockingOrDie();                                                  // 阻塞socket
        int Connect(int fd, const struct sockaddr_in &addr);                        // 连接到服务器
        struct sockaddr_in getLocalAddr(int sockfd);                                // 获取本地地址
        struct sockaddr_in getPeerAddr(int sockfd);                                 // 获取对端地址
        bool isSelfConnect(int sockfd);                                             // 判断是否是自连接
        int getSocketError(int sockfd);                                             // 获取socket错误码
        ssize_t read(int sockfd, void *buf, size_t count);
        ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt);
        ssize_t write(int sockfd, const void *buf, size_t count);
    } // namespace Sockets
} // namespace shanchuan
#endif // SOCKETSOPS_HPP