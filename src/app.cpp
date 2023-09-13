#include "app.h"
#include "log.h"
#include "global.h"
#include "worker.h"
#include "leader.h"
#include "acceptor.h"
#include "http_conn.h"
#include "inet_addr.h"

#include "nlohmann/json.hpp"

#include <set>
#include <memory>

std::unordered_map<int/*listen port*/, std::vector<app *> *> app::app_map_by_port;
std::unordered_map<std::string/*listen*/, int/*protocol*/> app::listen_set;
std::unordered_map<std::string/*host*/, app *> app::app_map_by_host;

int app::load_conf(nlohmann::json &apps) {
    std::set<std::string/*protocol:port*/> protocol_port_set;
    int i = 0;
    for (auto itor : apps) {
        i = app::app_map_by_host.size();
        std::unique_ptr<app::conf> cf(new app::conf());
        cf->listen = itor.value("listen", "");
        int port = 0;
        std::string ip;
        if (inet_addr::parse_ip_port(cf->listen, ip, port) == -1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is invalid!\n", i);
            return -1;
        }
        cf->host = itor.value("host", "");
        if (cf->host.length() == 0) {
            fprintf(stderr, "niubix: conf - apps[%d].host is empty!\n", i);
            return -1;
        }
        if (app::app_map_by_host.count(cf->host) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].host is duplicate, already exists!\n", i);
            return -1;
        }

        cf->policy = app::parse_policy(itor.value("prolicy", ""));
        if (cf->policy == -1) {
            fprintf(stderr, "niubix: conf - apps[%d].policy is invalid!\n", i);
            return -1;
        }

        std::string protocol = itor.value("protocol", "");
        if (protocol.length() == 0 || protocol != "http") {
            fprintf(stderr, "niubix: conf - apps[%d].protocol is invalid!\n", i);
            return -1;
        }
        std::string protocol_port = protocol + ":" + std::to_string(port);
        if (protocol_port_set.count(protocol_port) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen + protocol is duplicate, already exists!\n", i);
            return -1;
        }
        int protocolv = app::http_protocol;
        app::listen_set[cf->listen] = protocolv;

        cf->connect_backend_timeout = itor.value("connect_backend_timeout", -1);
        if (cf->connect_backend_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].connect_backend_timeout is invalid!\n", i);
            return -1;
        }
        cf->health_check_timeout = itor.value("health_check_timeout", -1);
        if (cf->health_check_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].health_check_timeout is invalid!\n", i);
            return -1;
        }
        cf->health_check_uri = itor.value("health_check_uri", "");
        if (cf->health_check_uri.length() > 0 && cf->protocol == app::http_protocol) {
            if (cf->health_check_uri[0] != '/') {
                fprintf(stderr, "niubix: conf - apps[%d].health_check_uri is invalid!\n", i);
                return -1;
            }
        }

        cf->with_x_forwarded_for = itor.value("x-forwarded-for", true);
        cf->with_x_real_ip = itor.value("x-real-ip", true);

        std::string backend_protocol = itor.value("backend_protocol", "");
        if (backend_protocol.length() == 0 || backend_protocol != "http") {
            fprintf(stderr, "niubix: conf - apps[%d].backend_protocol is invalid!\n", i);
            return -1;
        }

        nlohmann::json &backends = itor["backends"];
        if (backends.empty() || !backends.is_array()) {
            fprintf(stderr, "niubix: conf - apps[%d].backend is empty!\n", i);
            return -1;
        }
        int j = 0;
        for (auto bv : backends) {
            j = cf->backend_list.size();
            std::unique_ptr<app::backend> bp(new app::backend());
            if (cf->policy == app::weighted) {
                bp->weight = bv.value("weight", -1);
                if (bp->weight < 0) {
                    fprintf(stderr, "niubix: conf - apps[%d].backend[%d].weight is invalid!\n", i, j);
                    return -1;
                }
            }
            bp->host = bv.value("host", "");
            struct sockaddr_in taddr;
            if (bp->host.length() == 0 || inet_addr::parse_v4_addr(bp->host, &taddr) == -1) {
                fprintf(stderr, "niubix: conf - apps[%d].backend[%d].host is empty!\n", i, j);
                return -1;
            }
            cf->backend_list.push_back(bp.release());
        } // end of `for (auto bv : backends)'

        int total_w = 0;
        for (auto bp : cf->backend_list)
            total_w += bp->weight;
        if (total_w == 0) {
            fprintf(stderr, "niubix: conf - apps[%d] no valid(weight) backend!\n", i);
            return -1;
        }
        
        app *ap = new app();
        ap->cf = cf.release();
        app::app_map_by_host[ap->cf->host] = ap;

        std::vector<app *> *vp = app::app_map_by_port[port];
        if (vp == nullptr)
            vp = new std::vector<app *>();
        vp->push_back(ap);
        app::app_map_by_port[port] = vp;
    } // end of `for (auto itor : apps)'
    return 0;
}
// refer: https://tenfy.cn/2018/11/12/smooth-weighted-round-robin/
app::backend* app::smooth_wrr() {
    int total_w = 0;
    app::backend *best = nullptr;
    for (auto bp : this->cf->backend_list) {
        total_w += bp->weight;
        bp->current += bp->weight;
        if (best == nullptr || bp->current > best->current)
            best = bp;
    }
    best->current -= total_w;
    return best;
}
int app::run_all(const ::conf *cf) {
    worker *workers = g::g_leader->get_workers();
    for (const auto &kv : app::listen_set) {
        for (int i = 0; i < g::g_leader->get_worker_num(); ++i) {
            acceptor *acc = nullptr;
            worker *wrker = &(workers[i]);
            if (kv.second == app::http_protocol)
                acc = new acceptor(wrker, http_conn::new_conn_func);
            if (acc == nullptr) {
                log::error("invalid protocol %d", kv.second);
                return -1;
            }
            wrker->add_acceptor(acc);
            if (acc->open(kv.first, cf) == -1) {
                log::error("listen %s fail!", kv.first.c_str());
                return -1;
            }
        }
    }
    return 0;
}
