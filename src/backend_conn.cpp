#include "backend_conn.h"
#include "frontend_conn.h"
#include "worker.h"
#include "log.h"

#include <cerrno>
#include <string.h>

backend_conn::~backend_conn() {
}
bool backend_conn::on_open() {
    if (this->frontend == nullptr)
        return false;
    
    if (this->frontend->on_backend_connect_ok() != 0) {
        this->frontend = nullptr;
        return false;
    }

    if (this->wrker->add_ev(this, this->get_fd(), ev_handler::ev_read) != 0) {
        log::error("new backend conn add to poller fail! %s", strerror(errno));
        return false;
    }
    return true;
}
void backend_conn::on_connect_fail(const int err) {
    if (this->frontend == nullptr)
        return ;
    this->frontend->on_backend_connect_fail(err);
    this->frontend = nullptr;

    this->on_close();
}
void backend_conn::on_frontend_close() {
    if (this->frontend != nullptr)
        this->frontend = nullptr;
    
    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void backend_conn::on_close() {
    if (this->frontend != nullptr) {
        this->frontend->on_backend_close();
        this->frontend = nullptr;
    }
    this->destroy();
    delete this;
}
bool backend_conn::on_read() {
    char *buf = nullptr;
    int ret = this->recv(buf);
    if (ret == 0) // closed
        return false;
    else if (ret < 0)
        return true;

    this->frontend->send(buf, ret);
    return true;
}
