#include "leader.h"
#include "worker.h"
#include "options.h"
#include "ev_handler.h"

#include <errno.h>
#include <string.h>
#include <cstdio>
#include <thread>

int leader::open(const options &opt) {
    int cpu_num = std::thread::hardware_concurrency();
    this->worker_num = opt.worker_num;
    if (this->worker_num < 1)
        this->worker_num = cpu_num;
    
    this->workers = new worker[this->worker_num]();
    for (int i = 0; i < this->worker_num; ++i) {
        if (opt.set_cpu_affinity) {
            int cpu_id = i % cpu_num;
            this->workers[i].set_cpu_id(cpu_id);
        }
        if (this->workers[i].open(opt) != 0)
            return -1;
    }
    return 0;
}
int leader::add_ev(ev_handler *eh, const int fd, const uint32_t events) {
    if (fd < 0 || eh == nullptr || events == 0)
        return -1;
    int i = 0;
    if (this->worker_num > 1)
        i = fd % this->worker_num;
    return this->workers[i].add(eh, fd, events);
}
int leader::append_ev(const int fd, const uint32_t events) {
    if (fd < 0 || events == 0)
        return -1;
    int i = 0;
    if (this->worker_num > 1)
        i = fd % this->worker_num;
    return this->workers[i].append(fd, events);
}
int leader::remove_ev(const int fd, const uint32_t events) {
    if (fd < 0 || events == 0)
        return -1;
    int i = 0;
    if (this->worker_num > 1)
        i = fd % this->worker_num;
    return this->workers[i].remove(fd, events);
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
