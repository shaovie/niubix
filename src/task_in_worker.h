#ifndef NBX_TASK_IN_WORKER_H_
#define NBX_TASK_IN_WORKER_H_

#include "ringq.h"

class task_in_worker {
public:
    friend class async_taskq;
    enum {
        del_ev_handler      = 1,
        backend_conn_ok     = 2,
        backend_conn_fail   = 3,
        backend_close       = 4,
        frontend_close      = 5,
        close_acceptor      = 6,
        frontend_inactive   = 7,
        frontend_send_buffer_drained   = 8,
        backend_send_buffer_drained    = 9,

        gracefully_shutdown = 101,
    };
    task_in_worker() = default;
    task_in_worker(const int t, void *p): type(t), p(p) { }
    task_in_worker& operator=(const task_in_worker &v) {
        this->type = v.type;
        this->p = v.p;
        return *this;
    }

    static void do_tasks(ringq<task_in_worker> *taskq);
    static void do_task(const task_in_worker &t);
private:
    int type = 0;
    void *p = nullptr;
};
#endif // NBX_TASK_IN_WORKER_H_
