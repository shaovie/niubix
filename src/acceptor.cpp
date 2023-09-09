#include "acceptor.h"
#include "defines.h"
#include "socket.h"
#include "worker.h"
#include "conf.h"
#include "log.h"
#include "app.h"

#include <cstdio>
#include <string>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

int acceptor::open(const std::string &addr, const conf *cf) {
    if (addr.length() < 2) {
        log::error("accepor open fail! addr too short %s", addr.c_str());
        return -1;
    }
    if (addr.substr(0, 5) == "unix:")
        return this->uds_open(addr.substr(5, addr.length() - 5), cf);
    return this->tcp_open(addr, cf);
}
bool acceptor::on_read() {
    int fd = this->get_fd();
    int conn = -1;
    nbx_sockaddr_t peer_addr;
    socklen_t socklen = 0;
    for (int i = 0; i < 64; ++i) {
        socklen = sizeof(nbx_sockaddr_t);
        conn = ::accept4(fd, &peer_addr.sockaddr, &socklen, SOCK_NONBLOCK|SOCK_CLOEXEC);
        if (unlikely(conn == -1)) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EMFILE || errno == ENFILE) {
                // 句柄不足，暂停200毫秒，不然会一直触发事件
                if (this->wrker->schedule_timer(this, 200/*msec*/, 0) == 0)
                    this->wrker->remove_ev(fd, ev_handler::ev_all);
            }
            break;
        }
        auto eh = this->new_conn_func();
        if (unlikely(eh == nullptr)) {
            ::close(conn);
            break;
        }

        eh->set_remote_addr(&peer_addr.sockaddr, socklen);
        eh->set_fd(conn);
        if (eh->on_open() == false)
            eh->on_close();
    } // end of `for i < 128'
    return true;
}
bool acceptor::on_timeout(const int64_t) {
    if (this->get_fd() != -1)
        this->wrker->add_ev(this, this->get_fd(), ev_handler::ev_accept);
    return false;
}
void acceptor::close() {
    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->wrker->cancel_timer(this);
    this->destroy();
}
// addr ipv4: "192.168.0.1:8080" or ":8080"
// addr ipv6: "[2001:470:1f18:471::2]:8080" or "[]:8080"
int acceptor::tcp_open(const std::string &addr, const conf *cf) {
    struct sockaddr *listen_addr = nullptr;
    nbx_sockaddr_t laddr;
    ::memset(&laddr, 0, sizeof(laddr)); 
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int port = 0;
    std::string ip;
    if (app::parse_ip_port(addr, ip, port) == -1) {
        log::error("accepor open fail! tcp listen addr invalid %s", addr.c_str());
        return -1;
    }
    if (addr[0] == '[') { // ipv6
        addr_len = sizeof(struct sockaddr_in6);
        laddr.sockaddr_in6.sin6_family = AF_INET6;
        laddr.sockaddr_in6.sin6_port = ::htons(port);
        laddr.sockaddr_in6.sin6_addr = in6addr_any;
        if (ip.length() > 0)
            ::inet_pton(AF_INET6, ip.c_str(), &(laddr.sockaddr_in6.sin6_addr));
    } else {
        laddr.sockaddr_in.sin_family = AF_INET;
        laddr.sockaddr_in.sin_port = ::htons(port);
        laddr.sockaddr_in.sin_addr.s_addr = ::htonl(INADDR_ANY);
        if (ip.length() > 0)
            ::inet_pton(AF_INET, ip.c_str(), &(laddr.sockaddr_in.sin_addr));
    }
    if (port < 1 || port > 65535) {
        log::error("accepor open fail! port invalid");
        return -1;
    }

    int fd = ::socket(AF_INET6, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
    if (fd == -1) {
        log::error("create listen socket fail! %s", strerror(errno));
        return -1;
    }
    // `sysctl -a | grep net.ipv4.tcp_rmem` 返回 min default max
    // 默认内核会在min,max之间动态调整, default是初始值, 如果设置了SO_RCVBUF, 
    // 缓冲区大小不变成固定值, 内核也不会进行动态调整了.
    //
    // 必须在listen/connect之前调用
    // must < `sysctl -a | grep net.core.rmem_max`
    if (cf->rcvbuf_size > 0 && socket::set_rcvbuf(fd, cf->rcvbuf_size) != 0) {
        ::close(fd);
        log::error("set rcvbuf fail! %s", strerror(errno));
        return -1;
    }
    if (cf->reuse_addr && socket::reuseaddr(fd, 1) != 0) {
        ::close(fd);
        log::error("set reuseaddr fail! %s", strerror(errno));
        return -1;
    }
    if (cf->reuse_port && socket::reuseport(fd, 1) != 0) {
        ::close(fd);
        log::error("set reuseport fail! %s", strerror(errno));
        return -1;
    }
    this->listen_addr = addr;
    return this->listen(fd, listen_addr, addr_len, cf->backlog);
}
int acceptor::uds_open(const std::string &addr, const conf *cf) {
    struct sockaddr_un laddr;
    if (addr.length() > (sizeof(laddr.sun_path) - 1)) {
        log::error("unix socket addr too long! %s", addr.c_str());
        return -1;
    }
    ::remove(addr.c_str());
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        log::error("create unix socket fail! %s", strerror(errno));
        return -1;
    }
    laddr.sun_family = AF_UNIX;
    ::strncpy(laddr.sun_path, addr.c_str(), sizeof(laddr.sun_path) - 1);
    laddr.sun_path[sizeof(laddr.sun_path) - 1] = '\0';
    this->listen_addr = addr;
    return this->listen(fd,
        reinterpret_cast<sockaddr *>(&laddr), sizeof(laddr),
        cf->backlog);
}
int acceptor::listen(const int fd,
    const struct sockaddr *addr, socklen_t addrlen,
    const int backlog) {
    if (::bind(fd, addr, addrlen) == -1) {
        ::close(fd);
        log::error("bind fail! %s", strerror(errno));
        return -1;
    }

    if (::listen(fd, backlog) == -1) {
        ::close(fd);
        log::error("listen fail! %s", strerror(errno));
        return -1;
    }

    this->set_fd(fd);
    if (this->wrker->add_ev(this, fd, ev_handler::ev_accept) != 0) {
        ::close(fd);
        this->set_fd(-1);
        log::error("add accept ev handler fail! %s", strerror(errno));
        return -1;
    }
    return 0;
}
