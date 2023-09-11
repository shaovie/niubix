#include "leader.h"
#include "worker.h"
#include "conf.h"
#include "ev_handler.h"
#include "worker_timing_event.h"

#include <errno.h>
#include <string.h>
#include <cstdio>
#include <thread>

int leader::open(const conf *cf) {
    this->worker_num = cf->worker_num;
    this->workers = new worker[this->worker_num]();
    int cpu_num = std::thread::hardware_concurrency();
    for (int i = 0; i < this->worker_num; ++i) {
        if (cf->set_cpu_affinity) {
            int cpu_id = i % cpu_num;
            this->workers[i].set_cpu_id(cpu_id);
        }
        if (this->workers[i].open(this, cf) != 0)
            return -1;

        worker_cache_time *wct = new worker_cache_time(&this->workers[i]);
        this->workers[i].schedule_timer(wct, i, 80);
    }
    return 0;
}
int leader::add_ev(ev_handler *eh, const int fd, const uint32_t events) {
    if (fd < 0 || eh == nullptr || events == 0)
        return -1;
    int i = 0;
    if (this->worker_num > 1)
        i = fd % this->worker_num;
    return this->workers[i].add_ev(eh, fd, events);
}
int leader::append_ev(const int fd, const uint32_t events) {
    if (fd < 0 || events == 0)
        return -1;
    int i = 0;
    if (this->worker_num > 1)
        i = fd % this->worker_num;
    return this->workers[i].append_ev(fd, events);
}
int leader::remove_ev(const int fd, const uint32_t events) {
    if (fd < 0 || events == 0)
        return -1;
    int i = 0;
    if (this->worker_num > 1)
        i = fd % this->worker_num;
    return this->workers[i].remove_ev(fd, events);
}
void leader::run(const bool join) {
    if (!join) {
        for (int i = 0; i < this->worker_num; ++i) {
            std::thread thr(&worker::run, &(this->workers[i]));
            thr.detach();
        }
        return ;
    }
    std::thread **threads = new std::thread*[this->worker_num]();
    for (int i = 0; i < this->worker_num; ++i) {
        threads[i] = new std::thread(&worker::run, &(this->workers[i]));
    }
    for (int i = 0; i < this->worker_num; ++i) {
        threads[i]->join();
        delete threads[i];
    }
    delete[] threads;
}
