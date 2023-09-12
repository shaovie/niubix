#ifndef NBX_WORKER_TIMING_EVENT_H_
#define NBX_WORKER_TIMING_EVENT_H_

#include "ev_handler.h"
#include "worker.h"

class worker_cache_time : public ev_handler {
public:
    worker_cache_time(worker *w) { this->set_worker(w); }
    virtual bool on_timeout(const int64_t now) { this->wrker->now_msec = now; return true; }
};

class worker_stat_output : public ev_handler {
public:
    worker_stat_output(worker *w) { this->set_worker(w); }
    virtual bool on_timeout(const int64_t now);
};

#endif // NBX_WORKER_TIMING_EVENT_H_
