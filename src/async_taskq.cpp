#include "async_taskq.h"
#include "worker.h"
#include "log.h"

#include <cerrno>
#include <cstring>
#include <sys/eventfd.h>

async_taskq::~async_taskq() {
   if (this->efd != -1)
        ::close(this->efd);
    
   if (this->readq != nullptr)
       delete this->readq;
   if (this->writeq != nullptr)
       delete this->writeq;
}
int async_taskq::open() {
    int fd = ::eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    if (fd == -1) {
        log::error("create eventfd fail! %s", strerror(errno));
        return -1;
    }
    if (this->wrker->add_ev(this, fd, ev_handler::ev_read) == -1) {
        log::error("add eventfd to worker fail! %s", strerror(errno));
        ::close(fd);
        return -1;
    }
    this->efd = fd;
    return 0;
}
void async_taskq::push(const task_in_worker &t) {
    this->taskq_mtx.lock();
    this->writeq->push_back(t);
    this->taskq_mtx.unlock();

    int expected = 0;
    if (!this->notified.compare_exchange_strong(expected, 1))
        return ;
    int64_t v = 1;
    int ret = 0;
    do {
        ret = ::write(this->efd, (void *)&v, sizeof(v));
    } while (ret == -1 && errno == EINTR);
}
bool async_taskq::on_read() {
    if (this->readq->empty()) {
        this->taskq_mtx.lock();
        std::swap(this->readq, this->writeq);
        this->taskq_mtx.unlock();
    }
    int len = this->readq->length();
    for (auto i = 0; i < 64 && i < len; ++i) { // 1次最多处理64个
        task_in_worker &t = this->readq->front();
        this->do_task(t);
        
        this->readq->pop_front();
    }
    if (!this->readq->empty()) // Ignore readable eventfd, continue
        return true;

    int64_t v = 0;
    int ret = 0;
    do {
        ret = ::read(this->efd, (void *)&v, sizeof(v));
    } while (ret == -1 && errno == EINTR);

    this->notified.store(0);
    return true;
}
void async_taskq::do_task(const task_in_worker &t) {
    if (t.type == task_in_worker::gracefully_shutdown)
        this->wrker->gracefully_close();
}
