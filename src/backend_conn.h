#ifndef NBX_BACKEND_CONN_H_
#define NBX_BACKEND_CONN_H_

#include "io_handle.h"

// Forward declarations
class frontend_conn;

class backend_conn: public io_handle {
public:
    backend_conn(worker *w, frontend_conn *f): frontend(f) { this->set_worker(w); }
    virtual ~backend_conn();

    virtual bool on_open();

    virtual bool on_read();

    virtual void on_connect_fail(const int err);

    virtual void on_close();

    virtual void on_frontend_close();
private:
    frontend_conn *frontend = nullptr;
};

#endif // NBX_BACKEND_CONN_H_
