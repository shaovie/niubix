#ifndef NBX_ACCEPTOR_H_
#define NBX_ACCEPTOR_H_

#include "ev_handler.h"

#include <string>

// Forward declarations
class conf;
class worker;
struct sockaddr;

class acceptor : public ev_handler {
public:
    typedef ev_handler* (*new_conn_func_t)();

    acceptor() = delete;
    acceptor(worker *w, new_conn_func_t f) : new_conn_func(f) { this->set_worker(w); }

    // addr ipv4: "192.168.0.1:8080" or ":8080" or "unix:/tmp/xxxx.sock"
    // addr ipv6: "[::]:8080"
    int open(const std::string &addr, const conf *cf);
    void close(); // must called in worker thread
    void on_close(); // must called in worker thread
    const std::string &get_listen_addr() { return this->listen_addr; }

    virtual bool on_read();
    virtual bool on_timeout(const int64_t);
private:
    int tcp_open(const std::string &addr, const conf *cf);

    int uds_open(const std::string &addr, const conf *cf);

    int listen(const int fd,
        const struct sockaddr *addr, socklen_t addrlen,
        const int backlog);
public:
    int port = 0;
    std::string listen_addr;
    new_conn_func_t new_conn_func;
};

#endif // NBX_ACCEPTOR_H_
