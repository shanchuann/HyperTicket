#include <unistd.h> // close();
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/uio.h> // readv writev
#include "Logger.hpp"
#include "SocketsOps.hpp"

namespace shanchuan
{
    namespace Sockets
    {
        void setNonBlock(int sockfd) {
            int flags = ::fcntl(sockfd, F_GETFL, 0);
            flags |= O_NONBLOCK;
            int ret = ::fcntl(sockfd, F_SETFL, flags);
            flags = ::fcntl(sockfd, F_GETFD, 0);
            flags |= FD_CLOEXEC;
            ret = ::fcntl(sockfd, F_SETFD, flags);
        }
        void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr) {
            assert(ip != nullptr && addr != nullptr);
            addr->sin_family = AF_INET;
            addr->sin_port = htons(port); // htons: host to network short
            if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) LOG_SYSERR << "inet_pton error for " << ip;
        }
        void toIpPort(char *buff, size_t len, const struct sockaddr_in &addr) {
            assert(buff != nullptr);
            ::inet_ntop(AF_INET, &addr.sin_addr, buff, len);
            size_t pos = strlen(buff);
            uint16_t port = ntohs(addr.sin_port);
            assert(len > pos);
            snprintf(buff + pos, len - pos, ":%u", port);
        }
        void toIp(char *buff, size_t len, const struct sockaddr_in &addr) {
            assert(buff != nullptr);
            ::inet_ntop(AF_INET, &addr.sin_addr, buff, len);
        }
        void close(int sockfd) {
            if (::close(sockfd) < 0) LOG_SYSERR << "close error for " << sockfd;
        }
        int Connect(int sockfd, const struct sockaddr_in &addr) {
            return ::connect(sockfd, (struct sockaddr *)&addr,sizeof(addr));
        }
        void bindOrDie(int sockfd, const struct sockaddr_in &addr)
        {
            int ret = ::bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
            if (ret < 0) LOG_SYSFATAL << "bind error";
        }
        void listenOrDie(int sockfd) {
            int ret = ::listen(sockfd, SOMAXCONN);
            if (ret < 0) LOG_SYSFATAL << "listen error";
        }
        int accept(int sockfd, struct sockaddr_in *addr) {
            socklen_t addrlen = sizeof(*addr);
            int connfd = ::accept(sockfd, (struct sockaddr *)addr, &addrlen);
            if (connfd < 0) {
                int savedErrno = errno;
                LOG_SYSERR << "accept error";
                switch (savedErrno) {
                    case EAGAIN: case ECONNABORTED: case EINTR: case EPROTO: case EPERM:
                    case EMFILE: errno = savedErrno; break;
                    case EBADF: case EFAULT: case EINVAL: case ENFILE: case ENOBUFS: case ENOMEM: case ENOTSOCK:
                    case EOPNOTSUPP: LOG_FATAL << "unexpected error of ::accept " << savedErrno; break;
                    default: LOG_FATAL << "unknown error of ::accept " << savedErrno; break;
                }
            }
            return connfd;
        }
        void shutdownWrite(int sockfd) {
            LOG_TRACE<<"::shutdown(sockfd, SHUT_WR)";
            if (::shutdown(sockfd, SHUT_WR) < 0) LOG_SYSERR << "sockets::shutdownWrite";
        }
        int createNonblockingOrDie() {
            int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sockfd < 0) LOG_SYSFATAL << "sockets::createNonblockingOrDie";
            setNonBlock(sockfd);
            return sockfd;
        }
        int createBlockingOrDie() {
            int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sockfd < 0) LOG_SYSFATAL << "sockets::createNonblockingOrDie";
            return sockfd;
        }
        int getSocketError(int sockfd) {
            int optval = 0;
            socklen_t optlen = sizeof(optval);
            if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) return errno;
            else return optval;
        }
        struct sockaddr_in getLocalAddr(int sockfd)
        {
            struct sockaddr_in localaddr = {}; //  等价于 bzero(&localaddr, sizeof(localaddr));
            socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
            if (::getsockname(sockfd, (struct sockaddr *)&localaddr, &addrlen) < 0) LOG_SYSERR << "sockets::getLocalAddr fail " << strerror(errno);
            return localaddr;
        }
        struct sockaddr_in getPeerAddr(int sockfd) {
            struct sockaddr_in peeraddr = {}; // bzero(&peeraddr, sizeof(peeraddr));
            socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
            if (::getpeername(sockfd, (struct sockaddr *)&peeraddr, &addrlen) < 0) LOG_SYSERR << "sockets::getPeerAddr fail " << strerror(errno);
            return peeraddr;
        }
        bool isSelfConnect(int sockfd) {
            struct sockaddr_in localaddr = getLocalAddr(sockfd);
            struct sockaddr_in peeraddr = getPeerAddr(sockfd);
            return localaddr.sin_port == peeraddr.sin_port &&
                   localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr;
        }
        ssize_t read(int sockfd, void *buff, size_t count) {
            return ::read(sockfd, buff, count);
        }
        ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt) {
            return ::readv(sockfd, iov, iovcnt);
        }
        ssize_t write(int sockfd, const void *buf, size_t count) {
            return ::write(sockfd, buf, count);
        } 
    } // namespace Sockets
} // namespace shanchuan