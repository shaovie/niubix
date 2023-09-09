#include "app.h"
#include "log.h"
#include "global.h"
#include "worker.h"
#include "leader.h"
#include "acceptor.h"
#include "http_conn.h"

#include "nlohmann/json.hpp"

#include <set>
#include <memory>

std::map<std::string, int/*protocol*/> app::listen_set;
std::unordered_map<std::string/*host*/, app::conf *> app::app_map;

int app::load_conf(nlohmann::json &apps) {
    std::set<std::string> app_host_set;
    std::set<std::string/*protocol:port*/> protocol_port_set;
    int i = 0;
    for (auto itor : apps) {
        i = app::app_map.size();
        std::unique_ptr<app::conf> ap(new app::conf());
        ap->listen = itor.value("listen", "");
        int port = 0;
        std::string ip;
        if (app::parse_ip_port(ap->listen, ip, port) == -1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is invalid!\n", i);
            return -1;
        }
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
        app::listen_set[ap->listen] = protocolv;

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
        if (ap->health_check_uri.length() > 0 && ap->protocol == app::http_protocol) {
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
    for (const auto &kv : app::listen_set) {
        for (int i = 0; g::g_leader->get_worker_num(); ++i) {
            acceptor *acc = nullptr;
            if (kv.second == app::http_protocol)
                acc = new acceptor(&workers[i], http_conn::new_conn_func);
            if (acc->open(kv.first, cf) == -1) {
                log::error("listen %s fail!", kv.first.c_str());
                return -1;
            }
        }
    }
    return 0;
}
