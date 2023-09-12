#include "worker.h"
#include "connector.h"
#include "leader.h"
#include "conf.h"
#include "log.h"

#include <thread>
#include <cerrno>
#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/time.h>

int worker::open(leader *l, const int no, const conf *cf) {
    this->worker_no = no;
    this->poller = new evpoll();
    if (this->poller->open(cf->max_fds) != 0)
        return -1;

    this->rio_buf_size = cf->worker_io_buf_size;
    this->rio_buf = new char[this->rio_buf_size];
    this->wio_buf_size = cf->worker_io_buf_size;
    this->wio_buf = new char[this->wio_buf_size];

    this->timer = new timer_qheap(cf->timer_init_size);
    if (this->timer->open() == -1)
        return -1;

    if (this->poller->add(this->timer, this->timer->get_fd(), ev_handler::ev_read) != 0) {
        log::error("add timer to worker fail! %s", strerror(errno));
        return -1;
    }
    this->conn = new connector(this);
    this->ld = l;

    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    this->now_msec = int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000; // millisecond
    return 0;
}
int worker::close() {
    return 0;
}
void worker::set_cpu_affinity() {
    if (this->cpu_id == -1)
        return;
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(this->cpu_id, &cpu_set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set) != 0)
        log::error("set cpu affinity fail! %s", strerror(errno));
}
void worker::run() {
    this->thread_id = pthread_self();
    this->set_cpu_affinity();

    if (this->ld != nullptr)
        this->ld->worker_online();
    this->poller->run();
    if (this->ld != nullptr)
        this->ld->worker_offline();
}
