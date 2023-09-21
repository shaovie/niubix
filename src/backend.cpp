#include "backend.h"
#include "frontend.h"
#include "socket.h"
#include "worker.h"
#include "defines.h"
#include "log.h"
#include "app.h"

#include <cerrno>
#include <string.h>

// NOTE frontend & backend 不能在各自的执行栈中释放对方的资源,这样会导致资源过早释放
// poller中有ready_events 队列, 有可能backend另一个事件已经wait到了
// 交由taskq统一释放, 这样不受wait list影响
bool backend::on_open() {
    this->matched_app->backend_conn_ok_n.fetch_add(1, std::memory_order_relaxed);
    this->state = conn_ok;

    if (this->frontend_conn == nullptr) // 前端已经关闭, 自行释放资源
        return false;
    
    this->frontend_conn->backend_connect_ok();

    int fd = this->get_fd();
    socket::set_nodelay(fd);
    if (this->wrker->add_ev(this, fd, ev_handler::ev_read) != 0) {
        log::error("new backend conn add to poller fail! %s", strerror(errno));
        return false;
    }
    this->matched_app->backend_active_n.fetch_add(1, std::memory_order_relaxed);
    this->state = active_ok;
    return true;
}
void backend::on_connect_fail(const int /*err*/) {
    this->matched_app->backend_conn_fail_n.fetch_add(1, std::memory_order_relaxed);
    this->state = conn_fail;

    if (this->frontend_conn != nullptr) {
        this->frontend_conn->backend_connect_fail();
        this->frontend_conn = nullptr; // 解除关系
    }

    this->on_close();
}
void backend::frontend_close() {
    if (this->frontend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件

    this->frontend_conn = nullptr; // 好了, 现在跟前端没关系了, 前端已经关闭了
    this->wrker->push_task(task_in_worker(task_in_worker::frontend_close, this));
}
void backend::on_frontend_close() {
    if (this->state == new_ok || this->state == closed)
        return ; // 如果connector还没返回, 不做任何处理, 等待返回中处理结果
    
    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void backend::on_close() {
    if (this->state == active_ok)
        this->matched_app->backend_active_n.fetch_sub(1, std::memory_order_relaxed);
    if (this->frontend_conn != nullptr) {
        this->frontend_conn->backend_close();
        this->frontend_conn = nullptr; // 解除关系
    }
    this->state = closed;
    this->destroy();
    delete this;
}
bool backend::on_read() {
    if (this->frontend_conn == nullptr)
        return false;

    char *buf = nullptr;
    int ret = this->recv(buf);
    if (likely(ret > 0)) {
        this->frontend_conn->send(buf, ret);
        return true;
    }
    if (ret == 0) // closed
        return false;
    return true; // ret < 0
}
