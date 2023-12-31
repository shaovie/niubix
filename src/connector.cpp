#include "connector.h"
#include "inet_addr.h"
#include "defines.h"
#include "worker.h"
#include "log.h"

#include "socket.h"
#include "ev_handler.h"

class in_progress_connect final : public ev_handler {
public:
    friend connector;
    in_progress_connect(connector *cn, ev_handler *eh) : cn(cn), eh(eh) { }

    virtual bool on_read() {
        if (unlikely(this->io_ev_trigger == true))
            return true;
        this->eh->on_connect_fail(err_connect_fail);
        this->wrker->cancel_timer(this);
        this->io_ev_trigger = true;
        return false;
    }
    virtual bool on_write() {
        if (unlikely(this->io_ev_trigger == true))
            return true;
        int fd = this->get_fd();
        this->set_fd(-1); // From here on, the `fd` resources will be managed by eh.
        this->wrker->cancel_timer(this);
        this->io_ev_trigger = true;

        this->ok = true;
        this->eh->set_fd(fd);
        return false;
    }
    virtual bool on_timeout(const int64_t) {
        if (this->io_ev_trigger == true)
            return false;
        
        this->timeout_trigger = true;
        this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
        this->eh->on_connect_fail(err_connect_timeout);
        // 这里直接调用eh的on_connect_fail, 如果on_connect_fail中调用了remove_ev, 那可能会产生:
        // timerfd 和 eh->fd都在ready-events列表中, poll_desc提前将pd->eh置空, 导致ready-events列表
        // 中的eh为空了, 所以在epoll::run中要处理eh==nullptr的情况
        this->on_close();
        return false;
    }
    virtual void on_close() {
        // maybe trigger EPOLLHUP | EPOLLERR
        if (this->timeout_trigger == false && this->io_ev_trigger == false) {
            this->eh->on_connect_fail(err_connect_fail);
            this->wrker->cancel_timer(this);
        }
        this->destroy();

        // eh部分不能在on_write里边调用, 因为fd对应的eh变化了，在poller中fd对应的eh
        // 没有及时更新, 产生混乱
        if (this->ok == true && this->eh->on_open() == false)
            this->eh->on_close();

        delete this;
    }
private:
    bool ok = false;
    bool io_ev_trigger = false;
    bool timeout_trigger = false;
    connector *cn = nullptr;
    ev_handler *eh = nullptr;
};
// only ipv4 "192.168.1.101:8080"
int connector::connect(ev_handler *eh,
    const nbx_inet_addr &remote_addr,
    const int timeout /*milliseconds*/,
    const size_t rcvbuf_size) {

    int fd = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
    if (fd == -1)
        return -1;

    // `sysctl -a | grep net.ipv4.tcp_rmem` 返回 min default max
    // 默认内核会在min,max之间动态调整, default是初始值, 如果设置了SO_RCVBUF, 
    // 缓冲区大小不变成固定值, 内核也不会进行动态调整了.
    //
    // 必须在listen/connect之前调用
    // must < `sysctl -a | grep net.core.rmem_max`
    if (rcvbuf_size > 0 && socket::set_rcvbuf(fd, rcvbuf_size) == -1) {
        ::close(fd);
        return -1;
    }
    if (::connect(fd, remote_addr.addr, remote_addr.sock_len) == 0) {
        eh->set_fd(fd); // fd 所有权转移, 从此后eh管理fd
        if (eh->on_open() == false) {
            eh->on_close(); 
            return -1;
        }
        return 0;
    } else if (errno == EINPROGRESS) {
        if (timeout < 1) {
            ::close(fd);
            return -1;
        }
        return this->nblock_connect(eh, fd, timeout);
    }
    ::close(fd);
    return -1;
}
int connector::nblock_connect(ev_handler *eh, const int fd, const int timeout) {
    in_progress_connect *ipc = new in_progress_connect(this, eh);
    ipc->set_fd(fd);
    if (this->wrker->add_ev(ipc, fd, ev_handler::ev_connect) == -1) {
        delete ipc;
        ::close(fd);
        return -1;
    }
    // 要在同一个worker中注册定时器, 不然会有线程切换问题
    //
    // add_ev 和 schedule_timer 不保证原子性,
    // 有可能schedule_timer的时候 ipc已经触发了I/O事件
    ipc->wrker->schedule_timer(ipc, timeout, 0);
    return 0;
}
