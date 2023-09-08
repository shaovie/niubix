#ifndef POLLER_H_
#define POLLER_H_

#include "timer_qheap.h"

#include <pthread.h>
#include <cstdint>
#include <unordered_map>

// Forward declarations
class options;
class ev_handler; 

// worker 把poller融合在一起了, 这样层次简单一些
//
class worker {
public:
    worker() = default;

    int open(leader *l, const options &opt);

    //= timer
    inline int schedule_timer(ev_handler *eh, const int delay, const int interval) {
        return this->timer->schedule(eh, delay, interval);
    }
    inline void cancel_timer(ev_handler *eh) {
        this->timer->cancel(eh);
    }

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
private:
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
private:
    void set_cpu_id(const int id) { this->cpu_id = id; }
    void set_cpu_affinity();
    void destroy();
private:
    int cpu_id = -1;
    int io_buf_size = 0;
    char *io_buf = nullptr;
    timer_qheap *timer = nullptr;
    leader *myleader = nullptr;
    pthread_t thread_id;
    std::unordered_map<int, void *> pcache;
};

#endif // POLLER_H_
