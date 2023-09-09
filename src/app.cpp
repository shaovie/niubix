#include "app.h"
#include "worker.h"

int app::run_all(leader *l, const conf *cf) {
    worker *workers = l.get_workers();
    for (int i = 0; l.worker_num(); ++i) {
        //acceptor *acc = new acceptor(&workers[i], app
    }
}

    /*
    int poller_num = std::thread::hardware_concurrency();
    if (argc > 1)
        poller_num = atoi(argv[1]);

    signal(SIGPIPE ,SIG_IGN);

    g::g_reactor = new reactor();
    options opt;
    opt.set_cpu_affinity  = false;
    opt.with_timer_shared = true;
    opt.poller_num = 1;
    if (g::g_reactor->open(opt) != 0) {
        ::exit(1);
    }

    g::conn_reactor = new reactor();
    opt.set_cpu_affinity  = true;
    opt.with_timer_shared = false;
    opt.poller_num = poller_num;
    if (conn_reactor->open(opt) != 0) {
        ::exit(1);
    }

    opt.reuse_addr = true;
    for (int i = 0; i < opt.poller_num; ++i) {
        acceptor *acc = new acceptor(g::conn_reactor, gen_conn);
        if (acc->open(":8080", opt) != 0) {
            ::exit(1);
        }
    }
    g::conn_reactor->run(false);

    g::g_reactor->run(true);
    */
