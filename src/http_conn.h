#ifndef NBX_HTTP_CONN_H_
#define NBX_HTTP_CONN_H_

#include "frontend_conn.h"

// Forward declarations
class app;
class acceptor;
class backend_conn;

class http_conn: public frontend_conn {
public:
    http_conn() = default;
    virtual ~http_conn();
    static ev_handler *new_conn_func() { return new http_conn(); }

    virtual void set_acceptor(acceptor *a) { this->acc = a; };
    virtual void set_remote_addr(const struct sockaddr * /*addr*/, const socklen_t /*socklen*/);

    virtual bool on_open();

    virtual bool on_read();

    virtual void on_close();

    virtual int on_backend_connect_ok();
    virtual void on_backend_connect_fail(const int /*err*/);
    virtual void on_backend_close();
private:
    int to_connect_backend();
    bool handle_request(const char *buf, int len);
    int a_complete_request(const char *buf, const int len,
        const int header_line_end,
        const char *xff_start,
        const int xff_len);
    private:
    int partial_buf_len = 0;
    int local_addr_len = 0;
    int remote_addr_len = 0;
    socklen_t socklen = 0;
    int64_t start_time = 0;
    struct sockaddr *sockaddr = nullptr;
    acceptor *acc = nullptr;
    app *matched_app = nullptr;
    backend_conn *backend = nullptr;
    char *local_addr = nullptr;
    char *remote_addr = nullptr;
    char *partial_buf = nullptr;
};

#endif // NBX_HTTP_CONN_H_
