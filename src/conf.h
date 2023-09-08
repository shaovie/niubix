#ifndef NBX_CONF_H_
#define NBX_CONF_H_

#include <set>
#include <string>
#include <unordered_map>

#include "defines.h"

class conf {
public:
    class app {
    public:
        enum {
            roundrobin  = 1,
            weighted    = 2,
            random      = 3,
            iphash      = 4,
        };
        class backend {
        public:
            backend() = default;
            int weight = 0;
            std::string host;
        };
        app() = default;

        static int parse_policy(const std::string &s) {
            if (s == "roundrobin") return roundrobin;
            if (s == "weighted")   return weighted;
            if (s == "random")     return random;
            if (s == "iphash")     return iphash;
            return -1;
        }

        int policy = 0;
        int connect_backend_timeout = 1000; // msec
        int health_check_timeout    = 1000; // msec
        std::string protocol;
        std::string listen;
        std::string host;
        std::string health_check_uri;
        std::vector<backend *> backend_list;
    };
    conf() = default;

    int load(const char *path);
public:
    int worker_num = 0;
    int max_fds = 0;
    char pid_file[MAX_FILE_NAME_LENGTH] = {0};
    char log_dir[MAX_FILE_NAME_LENGTH] = {0};
    char log_level[MAX_FILE_NAME_LENGTH] = {0};
    char master_log[MAX_FILE_NAME_LENGTH] = {0};

    std::set<std::string> listen_set;
    std::unordered_map<std::string, app *> app_map;
};

#endif // NBX_CONF_H_

