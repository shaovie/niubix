#include "global.h"
#include "conf.h"
#include "log.h"
#include "worker.h"
#include "leader.h"
#include "worker_timing_event.h"

int g::pid = 0;
int g::shutdown_child_pid = 0;
int g::child_pid = 0;
worker *g::main_worker = nullptr;
leader *g::g_leader = nullptr;

int g::init(const conf *cf) {
    g::main_worker = new worker();
    if (g::main_worker->open(nullptr, 0, cf) != 0) {
        log::error("main worker open fail!");
        return -1;
    }
    worker_cache_time *wct = new worker_cache_time(g::main_worker);
    g::main_worker->schedule_timer(wct, 10, 48);

    worker_stat_output *wso = new worker_stat_output(g::main_worker);
    g::main_worker->schedule_timer(wso, 800, 1000);

    g::g_leader = new leader();
    if (g::g_leader->open(cf) != 0) {
        log::error("leader open fail!");
        return -1;
    }
    // 以上只是线程启动完成, 并没有附加事件处理
    return 0;
}
void g::let_worker_shutdown() {
    g::g_leader->gracefully_close_all();

    // timing check
    worker_shutdown *ws = new worker_shutdown(g::main_worker);
    g::main_worker->schedule_timer(ws, 10, 100);
}
