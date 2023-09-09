#include "app.h"
#include "global.h"
#include "worker.h"
#include "leader.h"
#include "acceptor.h"

#include "nlohmann/json.hpp"

#include <memory>

std::set<std::string> app::listen_set;
std::unordered_map<std::string/*host*/, app::conf *> app::app_map;

int app::load_conf(nlohmann::json &apps) {
    std::set<std::string> app_host_set;
    int i = 0;
    for (auto itor : apps) {
        i = app::app_map.size();
        std::unique_ptr<app::conf> ap(new app::conf());
        ap->listen = itor.value("listen", "");
        if (ap->listen.length() == 0) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is empty!\n", i);
            return -1;
        }
        if (app::listen_set.count(ap->listen) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is duplicate, already exists!\n", i);
            return -1;
        }
        app::listen_set.insert(ap->listen);

        ap->host = itor.value("host", "");
        if (ap->host.length() == 0) {
            fprintf(stderr, "niubix: conf - apps[%d].host is empty!\n", i);
            return -1;
        }
        if (app_host_set.count(ap->host) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].host is duplicate, already exists!\n", i);
            return -1;
        }
        app_host_set.insert(ap->host);

        ap->policy = app::parse_policy(itor.value("prolicy", ""));
        if (ap->policy == -1) {
            fprintf(stderr, "niubix: conf - apps[%d].policy is invalid!\n", i);
            return -1;
        }

        ap->protocol = itor.value("protocol", "");
        if (ap->protocol.length() == 0 || ap->protocol != "http") {
            fprintf(stderr, "niubix: conf - apps[%d].protocol is invalid!\n", i);
            return -1;
        }
        ap->connect_backend_timeout = itor.value("connect_backend_timeout", -1);
        if (ap->connect_backend_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].connect_backend_timeout is invalid!\n", i);
            return -1;
        }
        ap->health_check_timeout = itor.value("health_check_timeout", -1);
        if (ap->health_check_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].health_check_timeout is invalid!\n", i);
            return -1;
        }
        ap->health_check_uri = itor.value("health_check_uri", "");
        if (ap->health_check_uri.length() > 0 && ap->protocol == "http") {
            if (ap->health_check_uri[0] != '/') {
                fprintf(stderr, "niubix: conf - apps[%d].health_check_uri is invalid!\n", i);
                return -1;
            }
        }

        nlohmann::json &backends = itor["backends"];
        if (backends.empty() || !backends.is_array()) {
            fprintf(stderr, "niubix: conf - apps[%d].backend is empty!\n", i);
            return -1;
        }
        int j = 0;
        for (auto bv : backends) {
            j = ap->backend_list.size();
            std::unique_ptr<app::backend> bp(new app::backend());
            if (ap->policy == app::weighted) {
                bp->weight = bv.value("weight", -1);
                if (bp->weight < 0) {
                    fprintf(stderr, "niubix: conf - apps[%d].backend[%d].weight is invalid!\n", i, j);
                    return -1;
                }
            }
            bp->host = bv.value("host", "");
            if (bp->host.length() == 0 || bp->host.find(":") == std::string::npos) {
                fprintf(stderr, "niubix: conf - apps[%d].backend[%d].host is empty!\n", i, j);
                return -1;
            }
            ap->backend_list.push_back(bp.release());
        } // end of `for (auto bv : backends)'

        const std::string &host = ap->host;
        app::app_map[host] = ap.release();
    } // end of `for (auto itor : apps)'
    return 0;
}
int app::run_all(const ::conf *cf) {
    worker *workers = g::g_leader->get_workers();
    for (int i = 0; g::g_leader->get_worker_num(); ++i) {
        acceptor *acc = new acceptor(&workers[i], nullptr);
        acc->open("", cf);
    }
    return 0;
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
