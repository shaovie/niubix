#include "backend_conn.h"
#include "frontend_conn.h"
#include "worker.h"
#include "log.h"
#include "app.h"

#include <cerrno>
#include <string.h>

// NOTE frontend & backend 不能在各自的执行栈中释放对方的资源,这样会导致资源过早释放
// poller中有ready_events 队列, 有可能backend另一个事件已经wait到了
// 交由taskq统一释放, 这样不受wait list影响

backend_conn::~backend_conn() {
}
bool backend_conn::on_open() {
    this->matched_app->backend_conn_ok_n.fetch_add(1, std::memory_order_relaxed);
    this->connect_ret = true;

    if (this->frontend == nullptr) // 前端已经关闭, 自行释放资源
        return false;
    
    this->frontend->backend_connect_ok();

    this->matched_app->backend_active_n.fetch_add(1, std::memory_order_relaxed);
    if (this->wrker->add_ev(this, this->get_fd(), ev_handler::ev_read) != 0) {
        log::error("new backend conn add to poller fail! %s", strerror(errno));
        return false;
    }
    return true;
}
void backend_conn::on_connect_fail(const int /*err*/) {
    this->matched_app->backend_conn_fail_n.fetch_add(1, std::memory_order_relaxed);
    this->connect_ret = true;

    if (this->frontend != nullptr) {
        this->frontend->backend_connect_fail();
        this->frontend = nullptr; // 解除关系
    }

    this->on_close();
}
void backend_conn::frontend_close() {
    if (this->frontend != nullptr)
        this->frontend = nullptr; // 好了, 现在跟前端没关系了, 前端已经关闭了
    this->wrker->push_task(task_in_worker(task_in_worker::frontend_close, this));
}
void backend_conn::on_frontend_close() {
    // 如果connector还没返回, 不做任何处理, 等待返回中处理结果
    if (this->connect_ret == false)
        return ;
    
    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void backend_conn::on_close() {
    int c = this->matched_app->backend_active_n.fetch_sub(1, std::memory_order_relaxed);
    log::info("backend destroy %d backend_active_n=%d", this->get_fd(), c);
    if (this->frontend != nullptr) {
        this->frontend->backend_close();
        this->frontend = nullptr; // 解除关系
    }
    this->destroy();
    this->wrker->push_task(task_in_worker(task_in_worker::del_backend_conn, this));
}
bool backend_conn::on_read() {
    if (this->frontend == nullptr)
        return false;

    char *buf = nullptr;
    int ret = this->recv(buf);
    if (ret == 0) // closed
        return false;
    else if (ret < 0)
        return true;

    this->frontend->send(buf, ret);
    return true;
}
