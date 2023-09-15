#include "health_check.h"
#include "connector.h"
#include "defines.h"
#include "socket.h"
#include "worker.h"
#include "log.h"

class http_health_check_conn final : public io_handle {
public:
    http_health_check_conn(worker *w, http_health_check *ck, const std::string &h,
        const int to, const std::string &uri):
        health_check_timeout(to),
        chk(ck),
        host(h),
        health_check_uri(uri) {
        this->set_worker(w);
    }
    virtual ~http_health_check_conn() { };

    virtual bool on_open() {
        socket::set_nodelay(this->get_fd());
        if (this->wrker->add_ev(this, this->get_fd(), ev_handler::ev_read) != 0) {
            log::error("new http health check conn add to poller fail! %s", strerror(errno));
            return false;
        }
        this->my_timer_trigger = false;
        this->wrker->schedule_timer(this, this->health_check_timeout, 0);
        return this->send_check_msg();
    }
    virtual bool on_timeout(const int64_t /*now*/) {
        this->my_timer_trigger = true;
        this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
        this->on_close();
        return false;
    }
    virtual bool on_read() {
        char *buf = nullptr;
        int ret = this->recv(buf);
        if (likely(ret > 0)) {
            //TODO 等解析器完成后再解析code=200
            this->res_ok_time = this->wrker->now_msec;
            return false; // close
        }
        if (ret == 0) // closed
            return false;
        return true; // ret < 0
    }
    virtual void on_connect_fail(const int ) {
        log::info("backend %s connect fail!", this->host.c_str());
        this->chk->backend_offline();
        this->on_close();
    }
    virtual void on_close() { // maybe trigger EPOLLHUP | EPOLLERR
        if (this->res_ok_time > 0) {
            if (this->res_ok_time - this->req_time > this->health_check_timeout)
                this->chk->backend_offline();
            else
                this->chk->backend_online();
        } else {
            // connect fail or not response or response timeout
            // or conn exception closed, and include add_ev fail
            this->chk->backend_offline();
        }
        
        if (!this->my_timer_trigger)
            this->wrker->cancel_timer(this);
        this->destroy();
        this->chk->conn = nullptr;
        delete this;
    }

    int send_check_msg() {
        this->req_time = this->wrker->now_msec;
        char buf[1024];
        int ret = ::snprintf(buf, sizeof(buf),
            "GET %s HTTP/1.0\r\n"
            "Connection: close\r\n"
            "Server: niubix\r\n"
            "\r\n", this->health_check_uri.c_str());
        return this->send(buf, ret);
    }

    bool my_timer_trigger = true;
    int health_check_timeout = 0;
    int64_t req_time = 0;
    int64_t res_ok_time = 0;
    http_health_check *chk = nullptr;
    const std::string &host;
    const std::string &health_check_uri;
};
bool http_health_check::on_timeout(const int64_t ) {
    if (this->conn != nullptr)
        return true;
    http_health_check_conn *c = new http_health_check_conn(this->wrker, this,
        this->backend->host, this->backend->health_check_timeout,
        this->backend->health_check_uri);
    struct sockaddr_in taddr;
    inet_addr::parse_v4_addr(this->backend->host, &taddr);
    nbx_inet_addr naddr{(struct sockaddr*)&taddr, sizeof(taddr)};
    if (this->wrker->conn->connect(c, naddr, this->bapp->cf->connect_backend_timeout) == -1)
        delete c;
    else
        this->conn = c;
    return true;
}
