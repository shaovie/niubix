#ifndef NBX_ACCEPTOR_H_
#define NBX_ACCEPTOR_H_

#include "ev_handler.h"

#include <string.h>

// Forward declarations
class worker;
class options;
struct sockaddr;

class acceptor : public ev_handler {
public:
    typedef ev_handler* (*new_conn_func_t)();

    acceptor() = delete;
    acceptor(worker *w, new_conn_func_t f) : new_conn_func(f) { this->set_worker(w); }

    // addr ipv4: "192.168.0.1:8080" or ":8080" or "unix:/tmp/xxxx.sock"
    // addr ipv6: "[::]:8080"
    int open(const std::string &addr, const options &opt);
    void close(); // must called in worker thread
    const std::string &get_listen_addr() { return this->listen_addr; }

    virtual bool on_read();
    virtual bool on_timeout(const int64_t);
private:
    int tcp_open(const std::string &addr, const options &opt);

    int uds_open(const std::string &addr, const options &opt);

    int listen(const int fd,
        const struct sockaddr *addr, socklen_t addrlen,
        const int backlog);
private:
    std::string listen_addr;
    new_conn_func_t new_conn_func;
};

#endif // NBX_ACCEPTOR_H_
