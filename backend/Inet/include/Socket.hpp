#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/tcp.h>

struct tcp_info;

namespace shanchuan
{
    class InetAddress;
    class Socket
    {
    private:
        const int sockfd_;
    public:
        explicit Socket(int sockfd);
        ~Socket();
        int fd() const;
        void bindAddress(const InetAddress &localaddr);
        void listen();
        int accept(InetAddress *peeraddr);
        void shutdownWrite();
        void setTcpNoDelay(bool on);
        void setKeepAlive(bool on);
        bool getTcpInfo(struct tcp_info *) const;
        bool getTcpInfoString(char *buf,int len) const;
    };
} // namespace shanchuan
#endif // SOCKET_H