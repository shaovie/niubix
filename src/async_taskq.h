#ifndef NBX_ASYNC_TASKQ_H_
#define NBX_ASYNC_TASKQ_H_

#include "ev_handler.h"
#include "task_in_worker.h"

#include <mutex>
#include <atomic>

class async_taskq final : public ev_handler {
public:
    async_taskq() = delete;
    ~async_taskq();
    async_taskq(worker *w, const int init_size) {
        this->set_worker(w);
        this->readq  = new ringq<task_in_worker>(init_size);
        this->writeq = new ringq<task_in_worker>(init_size);
    }
    int open();

    virtual bool on_read() final;

    void push(const task_in_worker &t);
private:
    void do_task(const task_in_worker &t);

    int efd = -1;
    std::atomic<int> notified;
    ringq<task_in_worker> *readq  = nullptr;
    ringq<task_in_worker> *writeq = nullptr;
    std::mutex taskq_mtx;
};
#endif // NBX_ASYNC_TASKQ_H_
