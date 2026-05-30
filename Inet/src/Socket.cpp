#include <string.h>
#include "Logger.hpp"
#include "SocketsOps.hpp"
#include "InetAddress.hpp"
#include "Socket.hpp"

namespace shanchuan
{
    Socket::Socket(int sockfd) : sockfd_(sockfd) {
        LOG_TRACE << "create socket fd: " << sockfd_;
    }
    Socket::~Socket() {
        LOG_TRACE << "Socket::~Socket" << sockfd_;
        Sockets::close(sockfd_);
    }
    int Socket::fd() const { return sockfd_; }
    void Socket::bindAddress(const InetAddress &addr) {
        LOG_TRACE << "bind ..." << addr.toIpPort();
        LOG_TRACE << "bind fd .." << sockfd_;
        Sockets::bindOrDie(sockfd_, addr.getSockAddrInet());
    }
    void Socket::listen() {
        LOG_TRACE << "Socket::listen ...";
        Sockets::listenOrDie(sockfd_);
    }
    int Socket::accept(InetAddress *peeraddr) {
        struct sockaddr_in addr = {};
        // bzero(&addr,sizeof(addr);
        int connfd = Sockets::accept(sockfd_, &addr);
        if (connfd > 0) peeraddr->setSockAddrInet(addr);
        return connfd;
    }
    void Socket::shutdownWrite() {
        LOG_TRACE<<"Socket::shutdownWrite ...";
        Sockets::shutdownWrite(sockfd_);
    }
    void Socket::setTcpNoDelay(bool on) {
        int optval = on? 1:0;
        ::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY, &optval,static_cast<socklen_t>(sizeof(optval)));
    }
    void Socket::setKeepAlive(bool on) {
        int optval = (on? 1:0);
        ::setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE, &optval,static_cast<socklen_t>(sizeof(optval)));

    }
    bool Socket::getTcpInfo(struct tcp_info *tcpi) const {
        socklen_t len = sizeof(*tcpi);
        memset(tcpi,0,len);
        return ::getsockopt(sockfd_,SOL_TCP,TCP_INFO,tcpi,&len) == 0;
    }
    bool Socket::getTcpInfoString(char *buf,int len) const {
        struct tcp_info tcpi;
        bool ok = getTcpInfo(&tcpi);
        if (ok)
        {
          snprintf(buf, len, "unrecovered=%u "
                   "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                   "lost=%u retrans=%u rtt=%u rttvar=%u "
                   "sshthresh=%u cwnd=%u total_retrans=%u",
                   tcpi.tcpi_retransmits,       // Number of unrecovered [RTO] timeouts
                   tcpi.tcpi_rto,               // Retransmit timeout in usec
                   tcpi.tcpi_ato,               // Predicted tick of soft clock in usec
                   tcpi.tcpi_snd_mss,
                   tcpi.tcpi_rcv_mss,
                   tcpi.tcpi_lost,              // Lost packets
                   tcpi.tcpi_retrans,           // Retransmitted packets out
                   tcpi.tcpi_rtt,               // Smoothed round trip time in usec
                   tcpi.tcpi_rttvar,            // Medium deviation
                   tcpi.tcpi_snd_ssthresh,
                   tcpi.tcpi_snd_cwnd,
                   tcpi.tcpi_total_retrans);    // Total retransmits for entire connection
        }
        return ok;
    }
} // namespace shanchuan