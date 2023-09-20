#ifndef NBX_ADMIN_H_
#define NBX_ADMIN_H_

#include "io_handle.h"

#include <string>

// Forward declarations
class http_parser;

class admin : public io_handle {
public:
    admin() = default;
    ~admin();

    static ev_handler *new_admin_func() { return new admin(); }

    virtual void set_remote_addr(const struct sockaddr *addr, const socklen_t);
    virtual bool on_open();

    virtual bool on_read();

    virtual void on_close();
private:
    void shutdown();
    void reload();
    void set_backend_down(const char *query_start, const char *query_end);
    void set_backend_weight(const char *query_start, const char *query_end);
private:
    bool handle_request(const char *rbuf, int rlen);
    void response_error(const int code, const char *msg);
    void response_json(const std::string &data);
    int  a_complete_req(const http_parser &parser);

    int remote_addr_len = 0;
    char *remote_addr = nullptr;
};

#endif // NBX_ADMIN_H_
