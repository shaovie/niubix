#ifndef NBX_BACKEND_CONN_H_
#define NBX_BACKEND_CONN_H_

#include "io_handle.h"

// Forward declarations
class app;
class frontend_conn;

class backend_conn: public io_handle {
public:
    enum {
        new_ok      = 0,
        conn_ok     = 1,
        conn_fail   = 2,
        active_ok   = 3, // add ev to poll
        closed      = 4, // add ev to poll
    };
    backend_conn(worker *w, frontend_conn *f, app *ap):
        matched_app(ap),
        frontend(f)
    { this->set_worker(w); }
    virtual ~backend_conn();

    virtual bool on_open();

    virtual bool on_read();

    virtual void on_connect_fail(const int err);

    virtual void on_close();

    virtual void frontend_close();
    virtual void on_frontend_close();
private:
    int state = 0;
    app *matched_app = nullptr;
    frontend_conn *frontend = nullptr;
};

#endif // NBX_BACKEND_CONN_H_
