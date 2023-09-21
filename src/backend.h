#ifndef NBX_BACKEND_H_
#define NBX_BACKEND_H_

#include "io_handle.h"

// Forward declarations
class app;
class frontend;

class backend : public io_handle {
public:
    enum {
        new_ok      = 0,
        conn_ok     = 1,
        conn_fail   = 2,
        active_ok   = 3, // add ev to poll
        closed      = 4, // add ev to poll
    };
    backend(worker *w, frontend *f, app *ap):
        matched_app(ap),
        frontend_conn(f)
    { this->set_worker(w); }
    virtual ~backend() { }

    virtual bool on_open();

    virtual bool on_read();

    virtual void on_connect_fail(const int err);

    virtual void on_close();

    void frontend_close();
    void on_frontend_close();
private:
    char state = 0;
    app *matched_app = nullptr;
    frontend *frontend_conn = nullptr;
};

#endif // NBX_BACKEND_H_
