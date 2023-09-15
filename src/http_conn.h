#ifndef NBX_HTTP_CONN_H_
#define NBX_HTTP_CONN_H_

#include "frontend_conn.h"

// Forward declarations
class app;
class acceptor;
class backend_conn;

class http_conn: public frontend_conn {
public:
    enum {
        new_ok      = 0,
        conn_ok     = 1,
        active_ok   = 2, // add ev to poll
        closed      = 3, // add ev to poll
    };
    http_conn() = default;
    virtual ~http_conn();
    static ev_handler *new_conn_func() { return new http_conn(); }

    virtual void set_acceptor(acceptor *a) { this->acc = a; };
    virtual void set_remote_addr(const struct sockaddr * /*addr*/, const socklen_t /*socklen*/);

    virtual bool on_open();

    virtual bool on_read();

    virtual void on_close();

    virtual void backend_connect_ok();
    virtual void on_backend_connect_ok();

    virtual void backend_connect_fail();
    virtual void on_backend_connect_fail();

    virtual void backend_close();
    virtual void on_backend_close();
private:
    int to_connect_backend();
    bool handle_request(const char *buf, int len);
    bool handle_request2(const char *buf, int len);
    int a_complete_request(const char *buf, const int len,
        const int header_line_end,
        const bool has_x_real_ip,
        const char *xff_start,
        const int xff_len);
private:
    char state = 0;
    char method = 0;
    char local_addr_len = 0;
    char remote_addr_len = 0;
    int partial_buf_len = 0;
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
