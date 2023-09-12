#ifndef NBX_WORKER_H_
#define NBX_WORKER_H_

#include "evpoll.h"
#include "timer_qheap.h"

#include <pthread.h>
#include <cstdint>
#include <unordered_map>

// Forward declarations
class conf;
class leader;
class connector;
class ev_handler; 
class task_in_worker;

// worker 把poller融合在一起了, 这样层次简单一些
//
class worker {
public:
    friend class io_handle;
    friend class worker_cache_time;
    worker() = default;

    int open(leader *l, const int no, const conf *cf);
    int close();

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

    void push_task(const task_in_worker &t) { this->poller->push_task(t); }

    void init_poll_sync_opt(const int t, void *arg);
    void do_poll_sync_opt(const int t, void *arg);
    void poll_cache_set(const int id, void *val, void (*free_func)(void *)) {
        auto itor = this->pcache.find(id);
        if (itor != this->pcache.end())
            free_func(itor->second);
        this->pcache[id] = val;
    }
    void *poll_cache_get(const int id) {
        auto itor = this->pcache.find(id);
        if (itor != this->pcache.end())
            return itor->second;
        return nullptr;
    }
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
    pthread_t thread_id;
    std::unordered_map<int, void *> pcache;
};

#endif // NBX_WORKER_H_
