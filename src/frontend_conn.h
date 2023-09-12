#ifndef NBX_FRONTEND_CONN_H_
#define NBX_FRONTEND_CONN_H_

#include "io_handle.h"

// Forward declarations

class frontend_conn: public io_handle {
public:
    frontend_conn() = default;
    virtual ~frontend_conn() { };

    virtual void backend_connect_ok() = 0;
    virtual void on_backend_connect_ok() = 0;

    virtual void backend_connect_fail() = 0;
    virtual void on_backend_connect_fail() = 0;

    virtual void backend_close() = 0;
    virtual void on_backend_close() = 0;
};

#endif // NBX_FRONTEND_CONN_H_
