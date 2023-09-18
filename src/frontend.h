#ifndef NBX_FRONTEND_H_
#define NBX_FRONTEND_H_

#include "io_handle.h"

// Forward declarations

class frontend: public io_handle {
public:
    frontend() = default;
    virtual ~frontend() { };

    virtual void backend_connect_ok() = 0;
    virtual void on_backend_connect_ok() = 0;

    virtual void backend_connect_fail() = 0;
    virtual void on_backend_connect_fail() = 0;

    virtual void backend_close() = 0;
    virtual void on_backend_close() = 0;

    virtual void frontend_inactive() = 0;
    virtual void on_frontend_inactive() = 0;
};

#endif // NBX_FRONTEND_H_
