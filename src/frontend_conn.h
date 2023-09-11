#ifndef NBX_FRONTEND_CONN_H_
#define NBX_FRONTEND_CONN_H_

#include "io_handle.h"

// Forward declarations

class frontend_conn: public io_handle {
public:
    frontend_conn() = default;
    virtual ~frontend_conn() { };

    virtual int on_backend_connect_ok() { return -1; };
    virtual void on_backend_connect_fail(const int /*err*/) { };
    virtual void on_backend_close() { };
};

#endif // NBX_FRONTEND_CONN_H_
