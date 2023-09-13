#ifndef NBX_ROUTER_H_
#define NBX_ROUTER_H_

#include "nlohmann/json_fwd.hpp"

#include <string>
#include <atomic>
#include <unordered_map>

// Forward declarations
class conf;
class leader;

class app {
public:
    enum { // balance_policy 
        roundrobin  = 1,
        weighted    = 2,
        random      = 3,
        iphash      = 4,
    };
    enum {
     http_protocol = 1,
    };
    static int parse_policy(const std::string &s) {
        if (s == "roundrobin") return roundrobin;
        if (s == "weighted")   return weighted;
        if (s == "random")     return random;
        if (s == "iphash")     return iphash;
        return -1;
    }

    class backend;
    class conf {
    public:
        bool with_x_forwarded_for = true;
        bool with_x_real_ip = true;
        int policy = 0;
        int connect_backend_timeout = 1000; // msec
        int health_check_timeout    = 1000; // msec
        int protocol = 0;
        int backend_protocol = 0;
        std::string listen;
        std::string host;
        std::string health_check_uri;
        std::vector<backend *> backend_list;
    };

    class backend {
    public:
        backend() = default;

        int weight = 0;
        int current = 0;
        std::string host;
    };

    // smooth weighted round-robin balancing
    // https://github.com/phusion/nginx/commit/27e94984486058d73157038f7950a0a36ecc6e35
    backend *smooth_wrr();

    app::conf *cf = nullptr;
    std::atomic<int> accepted_num = {0};
    std::atomic<int> frontend_active_n = {0};
    std::atomic<int> backend_active_n = {0};
    std::atomic<int> backend_conn_ok_n = {0};
    std::atomic<int> backend_conn_fail_n = {0};
public:
    static int load_conf(nlohmann::json &);

    static int run_all(const ::conf *cf);

public:
    static std::unordered_map<int/*listen port*/, std::vector<app *> *> app_map_by_port;
    static std::unordered_map<std::string/*listen*/, int/*protocol*/> listen_set;
    static std::unordered_map<std::string/*host*/, app *> app_map_by_host;
};

#endif // NBX_ROUTER_H_
