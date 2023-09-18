#include "http_frontend.h"
#include "app.h"
#include "log.h"
#include "socket.h"
#include "backend.h"
#include "worker.h"
#include "acceptor.h"
#include "connector.h"
#include "defines.h"
#include "inet_addr.h"
#include "http.h"

#include <cstring>

http_frontend::~http_frontend() {
    if (this->partial_buf != nullptr)
        ::free(this->partial_buf);
    if (this->local_addr != nullptr)
        ::free(this->local_addr);
    if (this->remote_addr != nullptr)
        ::free(this->remote_addr);
}
void http_frontend::set_remote_addr(const struct sockaddr *addr, const socklen_t) {
    if (this->remote_addr == nullptr)
        this->remote_addr = (char *)::malloc(INET6_ADDRSTRLEN); // TODO optimize
    this->remote_addr[INET6_ADDRSTRLEN-1] = '\0';
    if (socket::addr_to_string(addr, this->remote_addr, INET6_ADDRSTRLEN) == 0)
        this->remote_addr_len = ::strlen(this->remote_addr);
}
bool http_frontend::on_open() {
    this->start_time = this->wrker->now_msec;
    this->state = conn_ok;

    if (this->local_addr == nullptr)
        this->local_addr = (char *)::malloc(INET6_ADDRSTRLEN); // TODO optimize
    this->local_addr[INET6_ADDRSTRLEN-1] = '\0';
    if (socket::get_local_addr(this->get_fd(), this->local_addr, INET6_ADDRSTRLEN) != 0) {
        log::error("new conn get local addr fail %s", strerror(errno));
        return this->response_err_and_close(HTTP_ERR_500);
    }

    this->local_addr_len = ::strlen(this->local_addr);
    auto app_l = app::app_map_by_port.find(this->acc->port);
    if (unlikely(app_l == app::app_map_by_port.end())) {
        log::info("new conn not match app by local port%d", this->acc->port);
        return this->response_err_and_close(HTTP_ERR_503);
    }
    auto vp = app_l->second;
    if (vp->size() == 1) { // 该端口只绑定了一个app, 立即准备连接后端
        this->matched_app = vp->front();
        if (this->to_connect_backend() != 0)
            return this->response_err_and_close(HTTP_ERR_503);
    }
    return true;
}
int http_frontend::to_connect_backend() {
    this->matched_app->accepted_num.fetch_add(1, std::memory_order_relaxed);
    app::backend *ab = nullptr;
    if (this->matched_app->cf->balance_policy == app::roundrobin)
        ab = this->matched_app->get_backend_by_smooth_wrr(); // no need to check for nullptr
    if (ab == nullptr)
        return -1;
    
    struct sockaddr_in taddr;
    inet_addr::parse_v4_addr(ab->host, &taddr);
    nbx_inet_addr naddr{(struct sockaddr*)&taddr, sizeof(taddr)};
    this->backend_conn = new backend(this->wrker, this, this->matched_app);
    if (this->wrker->conn->connect(this->backend_conn, naddr,
            this->matched_app->cf->connect_backend_timeout) == -1) {
        delete this->backend_conn;
        this->backend_conn = nullptr;
        log::info("connect to backend:%s fail!", ab->host.c_str());
        return -1;
    }
    return 0;
}
// NOTE frontend & backend 不能在各自的执行栈中操作对方的资源,这样会导致资源管理混乱
// poller中有ready_events 队列, 有可能backend另一个事件已经wait到了
// 交由taskq统一释放, 这样不受wait list影响
void http_frontend::backend_connect_ok() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件

    if (this->state == conn_ok)
        this->wrker->push_task(task_in_worker(task_in_worker::backend_conn_ok, this));
}
void http_frontend::on_backend_connect_ok() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件
    int fd = this->get_fd();
    if (fd == -1)
        return ;
    socket::set_nodelay(fd);
    if (this->wrker->add_ev(this, fd, ev_handler::ev_read) != 0) {
        log::error("new http_frontend add to poller fail! %s", strerror(errno));
        this->response_err_and_close(HTTP_ERR_500);
        this->on_close();
        return ;
    }
    this->matched_app->frontend_active_n.fetch_add(1, std::memory_order_relaxed);
    this->state = active_ok;
}
void http_frontend::backend_connect_fail() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件
    
    this->backend_conn = nullptr;
    this->wrker->push_task(task_in_worker(task_in_worker::backend_conn_fail, this));
}
void http_frontend::on_backend_connect_fail() {
    if (this->state == closed)
        return ;

    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void http_frontend::backend_close() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件
    
    this->backend_conn = nullptr;
    this->wrker->push_task(task_in_worker(task_in_worker::backend_close, this));
}
void http_frontend::on_backend_close() {
    if (this->state == closed)
        return ;

    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void http_frontend::on_close() { // maybe trigger EPOLLHUP | EPOLLERR
    if (this->matched_app != nullptr)
        if (this->state == active_ok)
            this->matched_app->frontend_active_n.fetch_sub(1, std::memory_order_relaxed);
    
    if (this->backend_conn != nullptr) {
        this->backend_conn->frontend_close();
        this->backend_conn = nullptr; // 解除关系
    }
    this->wrker->push_task(task_in_worker(task_in_worker::del_ev_handler, this));
    this->destroy();
    this->state = closed;
}
bool http_frontend::on_read() {
    if (unlikely(this->backend_conn == nullptr))
        return false;

    char *buf = nullptr;
    int ret = this->recv(buf);
    if (likely(ret > 0))
        return this->handle_request(buf, ret);
    if (ret == 0) // closed
        return false;
    return true; // ret < 0
}
bool http_frontend::response_err_and_close(const int errcode) {
    const char *sbf = http::err_msgs[errcode];
    this->send(sbf, ::strlen(sbf));
    return false;
}
