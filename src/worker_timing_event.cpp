#include "worker_timing_event.h"
#include "http_frontend.h"
#include "global.h"
#include "log.h"
#include "app.h"

bool worker_stat_output::on_timeout(const int64_t ) {
    for (auto ap : app::alls) {
        log::info("app:%s accepted_num=%d, frontend_active_n=%d, backend_active_n=%d"
            ", backend_conn_ok_n=%d, backend_conn_fail_n=%d", ap->cf->http_host.c_str(),
            ap->accepted_num.load(), ap->frontend_active_n.load(), ap->backend_active_n.load(),
            ap->backend_conn_ok_n.load(), ap->backend_conn_fail_n.load());
    }
    return true;
}
bool worker_shutdown::on_timeout(const int64_t ) {
    int tt_active_num = 0;
    for (auto ap : app::alls)
        tt_active_num += ap->frontend_active_n.load();
    
    if (tt_active_num < 1) {
        log::info("worker:%d gracefully exit", g::pid);
        ::exit(0);
    }
    return true;
}
bool worker_check_frontend_active::on_timeout(const int64_t now) {
    for (auto f : this->wrker->http_frontend_set) {
        if (f->state != http_frontend::active_ok)
            continue;

        // check idle
        if (f->recv_time == 0
            && now - f->start_time > f->matched_app->cf->frontend_idle_timeout) {
            f->frontend_inactive();
            continue ;
        } else if (f->recv_time > 0
            && now - f->recv_time > f->matched_app->cf->frontend_idle_timeout) {
            f->frontend_inactive();
            continue ;
        }

        // check req
        if (f->a_complete_req_time == 0
            && f->recv_time > 0
            && now - f->recv_time > f->matched_app->cf->frontend_a_complete_req_timeout) {
            f->frontend_inactive();
            continue ;
        } else if (f->a_complete_req_time > 0
            && f->recv_time > f->a_complete_req_time // a new req
            && now - f->recv_time > f->matched_app->cf->frontend_a_complete_req_timeout) {
            f->frontend_inactive();
            continue ;
        }
    }
    return true;
}
