#ifndef NBX_WORKER_H_
#define NBX_WORKER_H_

#include "evpoll.h"
#include "async_taskq.h"
#include "timer_qheap.h"

#include <set>
#include <vector>
#include <cstdint>
#include <pthread.h>
#include <unordered_map>

// Forward declarations
class conf;
class leader;
class acceptor;
class connector;
class ev_handler; 
class task_in_worker;
class http_frontend;

class worker_wakeup final : public ev_handler { 
public:
    worker_wakeup(worker *w);

    int open();
    virtual bool on_read() final;
    void wake();

    bool waked = false;
    int efd = -1;
};

// worker 把poller融合在一起了, 这样层次简单一些
//
class worker {
public:
    friend class io_handle;
    friend class worker_cache_time;
    worker() { this->acceptor_list.reserve(16); }

    int open(leader *l, const int no, const conf *cf);
    void gracefully_close();

    void add_acceptor(acceptor *acc) { this->acceptor_list.push_back(acc); }

    //= timer
    inline int schedule_timer(ev_handler *eh, const int delay, const int interval) {
        return this->timer->schedule(eh, delay, interval);
    }
    inline void cancel_timer(ev_handler *eh) { this->timer->cancel(eh); }

    //= io events
    inline int add_ev(ev_handler *eh, const int fd, const uint32_t ev) {
        eh->set_worker(this);
        return this->poller->add(eh, fd, ev);
    }
    inline int append_ev(const int fd, const uint32_t ev) {
        return this->poller->append(fd, ev);
    }
    inline int remove_ev(const int fd, const uint32_t ev) {
        return this->poller->remove(fd, ev);
    }

    //= event loop
    void run();

    void set_cpu_id(const int id) { this->cpu_id = id; }
    void set_cpu_affinity();
    void destroy();

    void push_task(const task_in_worker &t) {
        this->poller->push_task(t);
        this->wakeup->wake();
    }
    void push_async_task(const task_in_worker &t) { this->ataskq->push(t); }
public:
    int worker_no = 0;
    int cpu_id = -1;
    int rio_buf_size = 0;
    int wio_buf_size = 0;
    int64_t now_msec = 0;
    evpoll *poller = nullptr;
    char *rio_buf = nullptr;
    char *wio_buf = nullptr;
    timer_qheap *timer = nullptr;
    leader *ld = nullptr;
    connector *conn = nullptr;
    async_taskq *ataskq = nullptr;
    worker_wakeup *wakeup = nullptr;
    std::vector<acceptor *> acceptor_list;
    pthread_t thread_id;

    std::set<http_frontend *> http_frontend_set;
};

#endif // NBX_WORKER_H_
