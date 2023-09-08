#include "conf.h"

#include "nlohmann/json.hpp"

#include <string>
#include <memory>
#include <fstream>
#include <iostream>

int conf::load(const char *path) {
    std::ifstream f(path);
    nlohmann::json js;
    try {
        js = nlohmann::json::parse(f, nullptr, true, true);
    } catch (nlohmann::json::exception &e) {
        fprintf(stderr, "niubix: %s %s\n", path, e.what());
        return -1;
    }

    ::strncpy(this->master_log, js.value("master_log", "").c_str(), sizeof(this->master_log) - 1);
    this->master_log[sizeof(this->master_log) - 1] = '\0';
    if (::strlen(this->master_log) < 1) {
        fprintf(stderr, "niubix: conf - master_log invalid!\n");
        return -1;
    }

    ::strncpy(this->pid_file, js.value("pid_file", "").c_str(), sizeof(this->pid_file) - 1);
    this->pid_file[sizeof(this->pid_file) - 1] = '\0';
    if (::strlen(this->pid_file) < 1) {
        fprintf(stderr, "niubix: conf - pid_file invalid!\n");
        return -1;
    }
    this->max_fds = js.value("max_fds", 0);
	// The above are the master configuration,
    
    this->worker_num = js.value("worker_num", 0);
    ::strncpy(this->log_dir, js.value("log_dir", "").c_str(), sizeof(this->log_dir) - 1);
    this->log_dir[sizeof(this->log_dir) - 1] = '\0';
    if (::strlen(this->log_dir) < 1) {
        fprintf(stderr, "niubix: conf - log_dir invalid!\n");
        return -1;
    }

    ::strncpy(this->log_level, js.value("log_level", "").c_str(), sizeof(this->log_level) - 1);
    this->log_level[sizeof(this->log_level) - 1] = '\0';
    if (::strlen(this->log_level) < 1) {
        fprintf(stderr, "niubix: conf - log_level invalid!\n");
        return -1;
    }

    nlohmann::json &apps = js["apps"];
    if (apps.empty() || !apps.is_array()) {
        fprintf(stderr, "niubix: conf - apps is empty!\n");
        return -1;
    }
    std::set<std::string> app_host_set;
    int i = 0;
    for (auto itor : apps) {
        i = this->app_map.size();
        std::unique_ptr<conf::app> ap(new conf::app());
        ap->listen = itor.value("listen", "");
        if (ap->listen.length() == 0) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is empty!\n", i);
            return -1;
        }
        if (this->listen_set.count(ap->listen) == 1) {
            fprintf(stderr, "niubix: conf - apps[%d].listen is duplicate, already exists!\n", i);
            return -1;
        }
        this->listen_set.insert(ap->listen);

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

        ap->policy = conf::app::parse_policy(itor.value("prolicy", ""));
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
            std::unique_ptr<conf::app::backend> bp(new conf::app::backend());
            if (ap->policy == conf::app::weighted) {
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
        this->app_map[host] = ap.release();
    } // end of `for (auto itor : apps)'
    return 0;
}
