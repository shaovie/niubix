#include "worker_timing_event.h"
#include "log.h"
#include "app.h"

bool worker_stat_output::on_timeout(const int64_t ) {
    for (auto &kv : app::app_map_by_host) {
        auto ap = kv.second;
        log::info("app:%s accepted_num=%d, frontend_active_n=%d, backend_active_n=%d"
            ", backend_conn_ok_n=%d, backend_conn_fail_n=%d", ap->cf->host.c_str(),
            ap->accepted_num.load(), ap->frontend_active_n.load(), ap->backend_active_n.load(),
            ap->backend_conn_ok_n.load(), ap->backend_conn_fail_n.load());
    }
    return true;
}
