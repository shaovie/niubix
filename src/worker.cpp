#include "poll_sync_opt.h"
#include "worker.h"
#include "options.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sched.h>
#include <thread>

int worker::open(leader *l, const options &opt) {
    if (opt.worker_io_buf_size < 1) {
        fprintf(stderr, "niubix: worker_io_buf_size=%d < 1\n", opt.worker_io_buf_size);
        return -1;
    }
    if (opt.timer_init_size < 1) {
        fprintf(stderr, "niubix: timer_init_size=%d < 1\n", opt.timer_init_size);
        return -1;
    }
    if (opt.max_fds < 1) {
        fprintf(stderr, "niubix: max_fds=%d < 1\n", opt.max_fds);
        return -1;
    }
    this->poller = new evpoll();
    if (this->poller->open(opt.max_fds) != 0)
        return -1;

    this->io_buf_size = opt.worker_io_buf_size;
    this->io_buf = new char[this->io_buf_size];

    this->timer = new timer_qheap(opt.timer_init_size);
    if (timer->open() == -1)
        return -1;

    if (this->poller->add(timer, timer->get_fd(), ev_handler::ev_read) != 0) {
        fprintf(stderr, "niubix: add timer to worker fail! %s\n", strerror(errno));
        return -1;
    }
    this->myleader = l;
    return 0;
}
int worker::close() {
}
void worker::set_cpu_affinity() {
    if (this->cpu_id == -1)
        return;
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(this->cpu_id, &cpu_set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set) != 0)
        fprintf(stderr, "niubix: set cpu affinity fail! %s\n", strerror(errno));
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
