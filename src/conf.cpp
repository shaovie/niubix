#include "conf.h"
#include "app.h"

#include "nlohmann/json.hpp"

#include <thread>
#include <fstream>
#include <iostream>
#include <sys/resource.h>

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
    if (this->max_fds < 0) {
        fprintf(stderr, "niubix: conf - max_fds is invalid!\n");
        return -1;
    }
    if (this->max_fds == 0) {
        struct rlimit rlmt;
        if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
            fprintf(stderr, "niubix: get RLIMIT_NOFILE fail! %s\n", strerror(errno));
            return -1;
        }
        this->max_fds = rlmt.rlim_cur;
    }
	// The above are the master configuration,
    
    this->worker_num = js.value("worker_num", 0);
    if (this->worker_num == 0)
        this->worker_num = std::thread::hardware_concurrency();

    this->worker_io_buf_size = js.value("io_buf_size", 0);
    if (this->worker_io_buf_size < 1)
        this->worker_io_buf_size = 32*1024;
    
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

    ::strncpy(this->admin_listen, js.value("admin_listen", "").c_str(), sizeof(this->admin_listen) - 1);
    this->admin_listen[sizeof(this->admin_listen) - 1] = '\0';

    const std::string &ips = js.value("admin_ip_white_list", "");
    if (ips.length() > 0) {
        char *tok_p = NULL;
        char *token = NULL;
        char *bf = (char *)::malloc(ips.length() + 1);
        ::strncpy(bf, ips.c_str(), ips.length());
        bf[ips.length()] = '\0';
        for (token = ::strtok_r(bf, ",", &tok_p); 
            token != NULL;
            token = ::strtok_r(NULL, ",", &tok_p)) {
            this->admin_ip_white_set.insert(std::string(token));
        }
        ::free(bf);
    }

    nlohmann::json &apps = js["apps"];
    if (apps.empty() || !apps.is_array()) {
        fprintf(stderr, "niubix: conf - apps is empty!\n");
        return -1;
    }
    if (app::load_conf(apps) != 0) 
        return -1;
    
    return 0;
}
