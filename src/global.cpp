#include "global.h"
#include "conf.h"
#include "log.h"
#include "app.h"
#include "worker.h"
#include "leader.h"
#include "admin.h"
#include "acceptor.h"
#include "worker_timing_event.h"
#include "ssl.h"

#include <cstring>

bool g::worker_shutdowning = false;
int g::pid = 0;
int g::shutdown_child_pid = 0;
int g::child_pid = 0;
int g::master_pid = 0;
int64_t g::worker_start_time = 0;
const conf *g::cf = nullptr;
acceptor *g::admin_acceptor = nullptr;
worker *g::main_worker = nullptr;
leader *g::g_leader = nullptr;

int g::init(const conf *cf) {
    g::cf = cf;
    if (g::init_ssl() != 0)
        return -1;

    g::main_worker = new worker();
    if (g::main_worker->open(nullptr, 0, g::cf) != 0) {
        log::error("main worker open fail!");
        return -1;
    }
    if (::strlen(g::cf->admin_listen) > 1) {
        g::admin_acceptor = new acceptor(g::main_worker, admin::new_admin_func);
        // TODO use global conf ?
        if (g::admin_acceptor->open(std::string(g::cf->admin_listen), g::cf) == -1) {
            log::error("admin listen %s fail!", g::cf->admin_listen);
            return -1;
        }
    }

    worker_cache_time *wct = new worker_cache_time(g::main_worker);
    g::main_worker->schedule_timer(wct, 10, 48);

    worker_stat_output *wso = new worker_stat_output(g::main_worker);
    g::main_worker->schedule_timer(wso, 800, 1000);

    g::g_leader = new leader();
    if (g::g_leader->open(g::cf) != 0) {
        log::error("leader open fail!");
        return -1;
    }
    g::worker_start_time = ::time(nullptr);
    // 以上只是线程启动完成, 并没有附加事件处理
    return 0;
}
void g::let_worker_shutdown() {
    if (g::admin_acceptor != nullptr)
        g::admin_acceptor->close();
    g::g_leader->gracefully_close_all();

    // timing check
    worker_shutdown *ws = new worker_shutdown(g::main_worker);
    g::main_worker->schedule_timer(ws, 10, 100);
}
int g::init_ssl() {
    bool has_https_app = false;
    for (auto ap : app::alls) {
        if (ap->cf->protocol == app::https_protocol) {
            has_https_app = true;
            break;
        }
    }
    if (has_https_app == false)
        return 0;
    return ssl::init();
}
