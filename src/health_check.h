#ifndef NBX_HEALTH_CHECK_H_
#define NBX_HEALTH_CHECK_H_

#include "app.h"
#include "io_handle.h"

// Forward declarations
class worker;
class http_health_check_conn;

class http_health_check : public io_handle {
public:
    http_health_check(worker *w, app *ap, app::backend *ab): bapp(ap), backend(ab) {
        this->set_worker(w);
    }
    virtual ~http_health_check() { };

    virtual bool on_timeout(const int64_t );

    void backend_online()  { this->bapp->backend_online(this->backend);  }
    void backend_offline() { this->bapp->backend_offline(this->backend); }
private:
    int send_check_msg();

public:
    app *bapp = nullptr; // The app to which the backend belongs
    app::backend *backend = nullptr;
    http_health_check_conn *conn = nullptr;
};

#endif // NBX_HEALTH_CHECK_H_
