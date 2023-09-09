#ifndef NBX_HTTP_CONN_H_
#define NBX_HTTP_CONN_H_

#include "io_handle.h"

class http_conn: public io_handle {
public:
    http_conn() = default;
    static ev_handler *new_conn_func() { return new http_conn(); }

    virtual bool on_open();

    virtual bool on_read();

    virtual void on_close();
};

#endif // NBX_HTTP_CONN_H_

