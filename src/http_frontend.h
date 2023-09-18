#ifndef NBX_HTTP_FRONTEND_H_
#define NBX_HTTP_FRONTEND_H_

#include "frontend.h"

// Forward declarations
class app;
class acceptor;
class backend;
class http_parser;
class parse_req_result;

class http_frontend final : public frontend {
public:
    friend class worker_check_frontend_active;
    enum {
        new_ok      = 0,
        conn_ok     = 1,
        active_ok   = 2, // add ev to poll
        closed      = 3,
    };
    http_frontend() = default;
    virtual ~http_frontend();
    static ev_handler *new_conn_func() { return new http_frontend(); }

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

    virtual void frontend_inactive();
    virtual void on_frontend_inactive();
private:
    int to_connect_backend();
    bool response_err_and_close(const int errcode);
    bool handle_request(const char *buf, int len);
    void handle_partial_req(const http_parser &hp, const char *buf, const int rlen);
    bool filter_headers(const http_parser &parser, parse_req_result &prr);
    int a_complete_req(const parse_req_result &por);
private:
    char state = 0;
    char method = 0;
    char local_addr_len = 0;
    char remote_addr_len = 0;
    int partial_buf_len = 0;
    socklen_t socklen = 0;
    int64_t start_time = 0; //
    int64_t recv_time = 0;  // for app.frontend_idle_timeout
    int64_t a_complete_req_time = 0;  // for app.frontend_a_complete_req_timeout
    uint64_t content_length  = 0;
    acceptor *acc = nullptr;
    app *matched_app = nullptr;
    backend *backend_conn = nullptr;
    char *local_addr = nullptr;
    char *remote_addr = nullptr;
    char *partial_buf = nullptr;
};

#endif // NBX_HTTP_FRONTEND_H_
