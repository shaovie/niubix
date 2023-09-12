#include "task_in_worker.h"
#include "ev_handler.h"
#include "frontend_conn.h"
#include "backend_conn.h"

void task_in_worker::do_tasks(ringq<task_in_worker> *taskq) {
    int len = taskq->length();
    for (int i = 0; i < len; ++i) {
        task_in_worker &t = taskq->front();
        do_task(t);
        taskq->pop_front();
    }
}
void task_in_worker::do_task(const task_in_worker &t) {
    if (t.type == task_in_worker::del_ev_handler)
        delete (ev_handler *)t.p;
    else if (t.type == task_in_worker::backend_conn_ok)
        ((frontend_conn *)t.p)->on_backend_connect_ok();
    else if (t.type == task_in_worker::backend_conn_fail)
        ((frontend_conn *)t.p)->on_backend_connect_fail();
    else if (t.type == task_in_worker::backend_close)
        ((frontend_conn *)t.p)->on_backend_close();
    else if (t.type == task_in_worker::frontend_close)
        ((backend_conn *)t.p)->on_frontend_close();
}
