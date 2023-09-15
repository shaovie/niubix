#include "task_in_worker.h"
#include "ev_handler.h"
#include "frontend.h"
#include "backend.h"
#include "acceptor.h"
#include "log.h"

void task_in_worker::do_tasks(ringq<task_in_worker> *taskq) {
    int len = taskq->length();
    for (int i = 0; i < len; ++i) {
        task_in_worker &t = taskq->front();
        task_in_worker::do_task(t);
        taskq->pop_front();
    }
}
void task_in_worker::do_task(const task_in_worker &t) {
    switch (t.type) {
        case task_in_worker::del_ev_handler:
            delete (ev_handler *)t.p;
            break;
        case task_in_worker::backend_conn_ok:
            ((frontend *)t.p)->on_backend_connect_ok();
            break;
        case task_in_worker::backend_conn_fail:
            ((frontend *)t.p)->on_backend_connect_fail();
            break;
        case task_in_worker::backend_close:
            ((frontend *)t.p)->on_backend_close();
            break;
        case task_in_worker::frontend_close:
            ((backend *)t.p)->on_frontend_close();
            break;
        case task_in_worker::close_acceptor:
            ((acceptor *)t.p)->on_close();
            break;
        default:
            log::error("unknown worker task");
            break;
    }
}
