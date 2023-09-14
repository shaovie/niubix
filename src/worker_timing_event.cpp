#include "worker_timing_event.h"
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
    
    if (tt_active_num < 1)
        ::exit(0);
    return true;
}
