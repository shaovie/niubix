#include "app.h"
#include "log.h"
#include "global.h"
#include "worker.h"
#include "leader.h"
#include "acceptor.h"
#include "http_frontend.h"
#include "inet_addr.h"
#include "health_check.h"

#include "nlohmann/json.hpp"

#include <set>
#include <memory>

std::unordered_map<int/*listen port*/, std::vector<app *> *> app::app_map_by_port;
std::unordered_map<std::string/*listen*/, int/*protocol*/> app::listen_set;
std::vector<app *> app::alls;

int app::load_conf(nlohmann::json &apps) {
    std::set<std::string/*protocol:port*/> protocol_port_set;
    std::set<std::string/*host*/> app_host_set;
    std::set<std::string/*name*/> app_name_set;
    int i = 0;
    for (auto itor : apps) {
        i = app::alls.size();
        std::unique_ptr<app::conf> cf(new app::conf());
        cf->name = itor.value("name", "");
        if (cf->name.length() == 0) {
            fprintf(stderr, "niubix: conf - apps[%d].name is empty!\n", i);
            return -1;
        }
        if (app_name_set.count(cf->name) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].name is duplicate!\n", i);
            return -1;
        }
        app_name_set.insert(cf->name);

        cf->listen = itor.value("listen", "");
        int port = 0;
        std::string ip;
        if (inet_addr::parse_ip_port(cf->listen, ip, port) == -1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is invalid!\n", i);
            return -1;
        }
        cf->http_host = itor.value("http_host", "");
        if (cf->http_host.length() == 0) {
            fprintf(stderr, "niubix: conf - apps[%d].host is empty!\n", i);
            return -1;
        }
        if (app_host_set.count(cf->http_host) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].host is duplicate!\n", i);
            return -1;
        }

        cf->balance_policy = app::parse_balance_policy(itor.value("balance_policy", ""));
        if (cf->balance_policy == -1) {
            fprintf(stderr, "niubix: conf - apps[%d].balance_policy is invalid!\n", i);
            return -1;
        }

        std::string protocol = itor.value("protocol", "");
        if (protocol.length() == 0 || protocol != "http") {
            fprintf(stderr, "niubix: conf - apps[%d].protocol is invalid!\n", i);
            return -1;
        }
        std::string protocol_port = protocol + ":" + std::to_string(port);
        if (protocol_port_set.count(protocol_port) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen + protocol is duplicate!\n", i);
            return -1;
        }
        protocol_port_set.insert(protocol_port); // 相同协议+端口 不能重复
        cf->protocol = app::http_protocol; // TODO
        app::listen_set[cf->listen] = cf->protocol;

        cf->connect_backend_timeout = itor.value("connect_backend_timeout", 1000);
        if (cf->connect_backend_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].connect_backend_timeout is invalid!\n", i);
            return -1;
        }
        cf->frontend_a_complete_req_timeout = itor.value("frontend_a_complete_req_timeout", 5000);
        if (cf->frontend_a_complete_req_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].frontend_a_complete_req_timeout is invalid!\n", i);
            return -1;
        }
        cf->frontend_idle_timeout = itor.value("frontend_idle_timeout", 10000);
        if (cf->frontend_idle_timeout < 1) {
            fprintf(stderr, "niubix: conf - apps[%d].frontend_idle_timeout is invalid!\n", i);
            return -1;
        }
        cf->with_x_forwarded_for = itor.value("x-forwarded-for", true);
        cf->with_x_real_ip = itor.value("x-real-ip", true);

        std::string backend_protocol = itor.value("backend_protocol", "");
        if (backend_protocol.length() == 0 || backend_protocol != "http") {
            fprintf(stderr, "niubix: conf - apps[%d].backend_protocol is invalid!\n", i);
            return -1;
        }
        cf->backend_protocol = app::http_protocol; // TODO

        nlohmann::json &backends = itor["backends"];
        if (backends.empty() || !backends.is_array()) {
            fprintf(stderr, "niubix: conf - apps[%d].backend is empty!\n", i);
            return -1;
        }
        int j = 0;
        for (auto bv : backends) {
            j = cf->backend_list.size();
            std::unique_ptr<app::backend> bp(new app::backend());
            if (cf->balance_policy == app::roundrobin) {
                bp->weight = bv.value("weight", 1);
                if (bp->weight < 0) {
                    fprintf(stderr,
                        "niubix: conf - apps[%d].backend[%d].weight is invalid!\n", i, j);
                    return -1;
                }
            }
            bp->down = bv.value("down", false);
            bp->host = bv.value("host", "");
            struct sockaddr_in taddr;
            if (bp->host.length() == 0 || inet_addr::parse_v4_addr(bp->host, &taddr) == -1) {
                fprintf(stderr, "niubix: conf - apps[%d].backend[%d].host is empty!\n", i, j);
                return -1;
            }
            bp->health_check_period = bv.value("health_check_period", 0);
            if (bp->health_check_period > 0) {
                bp->health_check_timeout = bv.value("health_check_timeout", 0);
                if (bp->health_check_timeout < 1) {
                    fprintf(stderr,
                        "niubix: conf - apps[%d].health_check_timeout is invalid!\n", i);
                    return -1;
                }
                bp->health_check_uri = bv.value("health_check_uri", "");
                if (bp->health_check_uri.length() > 0
                    && cf->backend_protocol == app::http_protocol) {
                    if (bp->health_check_uri[0] != '/') {
                        fprintf(stderr,
                            "niubix: conf - apps[%d].health_check_uri is invalid!\n", i);
                        return -1;
                    }
                    if (bp->health_check_uri.length() > 512) {
                        fprintf(stderr,
                            "niubix: conf - apps[%d].health_check_uri too long! must < 512\n", i);
                        return -1;
                    }
                }
            }

            cf->backend_list.push_back(bp.release());
        } // end of `for (auto bv : backends)'

        int total_w = 0;
        for (auto bp : cf->backend_list)
            total_w += bp->weight;
        if (cf->balance_policy == app::roundrobin && total_w == 0) {
            fprintf(stderr, "niubix: conf - apps[%d] no valid(weight) backend!\n", i);
            return -1;
        }
        
        app *ap = new app();
        ap->cf = cf.release();
        app::alls.push_back(ap);
        app_host_set.insert(ap->cf->http_host);

        std::vector<app *> *vp = app::app_map_by_port[port];
        if (vp == nullptr)
            vp = new std::vector<app *>();
        vp->push_back(ap);
        app::app_map_by_port[port] = vp;
    } // end of `for (auto itor : apps)'
    return 0;
}
void app::backend_offline(app::backend *ab) {
    this->backend_mtx.lock(); // health_check 发生不频繁 共用一把大锁就可以了
    ab->health_check_ok_times = 0;
    ab->offline = true;
    this->backend_mtx.unlock();
}
void app::backend_online(app::backend *ab) {
    this->backend_mtx.lock();
    if (++(ab->health_check_ok_times) > 2)
        ab->offline = false;
    this->backend_mtx.unlock();
}
// https://github.com/phusion/nginx/commit/27e94984486058d73157038f7950a0a36ecc6e35
// refer: https://tenfy.cn/2018/11/12/smooth-weighted-round-robin/
app::backend* app::get_backend_by_smooth_wrr() {
    int total_w = 0;
    app::backend *best = nullptr;
    this->backend_mtx.lock();
    for (auto bp : this->cf->backend_list) {
        if (bp->down == true || bp->weight < 1 || bp->offline == true)
            continue;
        total_w += bp->weight;
        bp->current += bp->weight;
        if (best == nullptr || bp->current > best->current)
            best = bp;
    }
    if (best != nullptr) {
        best->current -= total_w;
        ++(best->counts);
    }
    this->backend_mtx.unlock();
    return best;
}
int app::run_all(const ::conf *cf) {
    worker *workers = g::g_leader->get_workers();
    for (const auto &kv : app::listen_set) {
        for (int i = 0; i < g::g_leader->get_worker_num(); ++i) {
            acceptor *acc = nullptr;
            worker *wrker = &(workers[i]);
            if (kv.second == app::http_protocol)
                acc = new acceptor(wrker, http_frontend::new_conn_func);
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

    // health check
    for (auto ap : app::alls) {
        for (auto ab : ap->cf->backend_list) {
            if (ab->health_check_period < 1)
                continue;
            if (ap->cf->backend_protocol == app::http_protocol) {
                if (ab->health_check_uri.empty())
                    continue;
                http_health_check *hhc = new http_health_check(g::main_worker, ap, ab);
                g::main_worker->schedule_timer(hhc, 100, ab->health_check_period);
            }
        }
    }
    return 0;
}
