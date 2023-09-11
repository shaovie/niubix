#ifndef NBX_SOCKET_H_
#define NBX_SOCKET_H_

#include "inet_addr.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

class socket
{
public:
    static int recv(const int fd, void *buff, const size_t len) {
        int ret = 0;
        do {
            ret = ::recv(fd, buff, len, 0);
        } while (ret == -1 && errno == EINTR);
        return ret;
    }
    static int send(const int fd, const void *buff, const size_t len) {
        int ret = 0;
        do {
            ret = ::send(fd, buff, len, 0);
        } while (ret == -1 && errno == EINTR);
        return ret;
    }
    static inline int close(const int fd) { return ::close(fd); }

    static inline unsigned short get_port(const struct sockaddr *addr) {
        if (addr->sa_family == AF_INET) {
            struct sockaddr_in *v4 = (struct sockaddr_in *)addr;
            return ::ntohl(v4->sin_port);
        } else if (addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)addr;
            return ::ntohl(v6->sin6_port);
        }
        return 0;
    }
    static inline int addr_to_string(const struct sockaddr *addr, char *buf, const int buf_len) {
        const char *p = nullptr;
        if (addr->sa_family == AF_INET) {
            p = ::inet_ntop(AF_INET,
                (void *)&(((struct sockaddr_in*)addr)->sin_addr),
                buf, buf_len);
        } else if (addr->sa_family == AF_INET6) {
            if (buf_len < INET6_ADDRSTRLEN)
                return -1;
            p = ::inet_ntop(AF_INET6,
                (void *)&(((struct sockaddr_in6*)addr)->sin6_addr),
                buf, buf_len);
        }
        return p != nullptr ? 0 : -1;
    }
    static inline int get_local_addr(const int fd, char *buf, const int buf_len) {
        nbx_sockaddr_t addr;
        socklen_t slen = sizeof(nbx_sockaddr_t);
        if (::getsockname(fd, &(addr.sockaddr), &slen) == -1)
            return -1;
        return socket::addr_to_string(&(addr.sockaddr), buf, buf_len);
    }
    static inline int reuseaddr(const int fd, const int val) {
        return ::setsockopt(fd,
            SOL_SOCKET,
            SO_REUSEADDR, 
            (const void*)&val,
            sizeof(val));
    }
    static inline int reuseport(const int fd, const int val) {
        return ::setsockopt(fd,
            SOL_SOCKET,
            SO_REUSEPORT, 
            (const void*)&val,
            sizeof(val));
    }
    static inline int set_nonblock(const int fd) {
        int flag = ::fcntl(fd, F_GETFL, 0);
        if (flag == -1) return -1;
        if (flag & O_NONBLOCK) // already nonblock
            return 0;
        return ::fcntl(fd, F_SETFL, flag | O_NONBLOCK);
    }
    static inline int set_block(const int fd) {
        int flag = ::fcntl(fd, F_GETFL, 0);
        if (flag == -1) return -1;
        if (flag & O_NONBLOCK) // already nonblock
            return ::fcntl(fd, F_SETFL, flag & (~O_NONBLOCK));
        return 0;
    }
    static inline int set_rcvbuf(const int fd, const size_t size) {
        if (size == 0) return -1;
        return ::setsockopt(fd,
            SOL_SOCKET, 
            SO_RCVBUF, 
            (const void*)&size, 
            sizeof(size));
    }
    static inline int set_sndbuf(const int fd, const size_t size) {
        if (size == 0) return -1;
        return ::setsockopt(fd,
            SOL_SOCKET,
            SO_SNDBUF, 
            (const void*)&size, 
            sizeof(size));
    }
    static inline int set_nodelay(const int fd) {
        int flag = 1;
        return ::setsockopt(fd,
            IPPROTO_TCP,
            TCP_NODELAY, 
            (void *)&flag, 
            sizeof(flag));
    }
    static inline int getsock_error(const int fd, int &err) {
        socklen_t len = sizeof(int);
        return ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    }
};

#endif // NBX_SOCKET_H_
