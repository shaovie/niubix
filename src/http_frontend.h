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

    void forward_to_backend(const char *buf, const int len);
private:
    int  to_connect_backend();
    bool response_err_and_close(const int errcode);
    bool handle_data(const char *buf, int len);
    bool handle_request(const char *buf, int len);
    int  handle_chunk(const char *rbuf, int rlen);
    int  handle_first_chunk(http_parser &parser, parse_req_result &prr);
    void handle_partial_req(const char *req_start, const int left_len);
    int  filter_headers(const http_parser &parser, parse_req_result &prr);
    void a_complete_req(parse_req_result &prr);
    bool to_match_app_by_host();
    void save_received_data_before_match_app(const char *buf, const int len);
private:
    bool transfer_chunked = false;
    char state = 0;
    char method = 0;
    char local_addr_len = 0;
    char remote_addr_len = 0;
    int partial_buf_len = 0;
    int received_data_len_before_match_app = 0;
    socklen_t socklen = 0;
    int64_t start_time = 0; //
    int64_t recv_time = 0;  // for app.frontend_idle_timeout
    int64_t a_complete_req_time = 0;  // for app.frontend_a_complete_req_timeout
    uint64_t content_length  = 0;
    uint64_t chunk_size  = 0;
    acceptor *acc = nullptr;
    app *matched_app = nullptr;
    backend *backend_conn = nullptr;
    char *local_addr = nullptr;
    char *remote_addr = nullptr;
    char *partial_buf = nullptr;
    char *received_data_before_match_app = nullptr;
    char *host = nullptr;
};

#endif // NBX_HTTP_FRONTEND_H_
