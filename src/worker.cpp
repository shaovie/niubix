#include "worker.h"
#include "leader.h"
#include "conf.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sched.h>
#include <thread>

int worker::open(leader *l, const conf *cf) {
    this->poller = new evpoll();
    if (this->poller->open(cf->max_fds) != 0)
        return -1;

    this->io_buf_size = cf->worker_io_buf_size;
    this->io_buf = new char[this->io_buf_size];

    this->timer = new timer_qheap(cf->timer_init_size);
    if (timer->open() == -1)
        return -1;

    if (this->poller->add(timer, timer->get_fd(), ev_handler::ev_read) != 0) {
        log::error("add timer to worker fail! %s", strerror(errno));
        return -1;
    }
    this->myleader = l;
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

    if (this->myleader != nullptr)
        this->myleader->worker_online();
    this->poller->run();
    if (this->myleader != nullptr)
        this->myleader->worker_offline();
}
