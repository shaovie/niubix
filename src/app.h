#ifndef NBX_ROUTER_H_
#define NBX_ROUTER_H_

#include "nlohmann/json_fwd.hpp"

#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>

// Forward declarations
class conf;;

class app {
public:
    enum { // balance_policy 
        roundrobin  = 1,
        random      = 2,
        iphash      = 3,
    };
    enum {
     http_protocol = 1,
    };
    static int parse_balance_policy(const std::string &s) {
        if (s == "roundrobin") return roundrobin;
        if (s == "random")     return random;
        if (s == "iphash")     return iphash;
        return -1;
    }

    class backend;
    class conf {
    public:
        bool with_x_forwarded_for = true;
        bool with_x_real_ip = true;
        int balance_policy  = 0;
        int connect_backend_timeout = 1000; // msec
        int frontend_a_complete_req_timeout = 5000; // msec
        int frontend_idle_timeout = 10000; // msec
        int protocol = 0;
        int backend_protocol = 0;
        std::string name;
        std::string listen;
        std::string http_host;
        std::vector<backend *> backend_list; // dynamic data, need mutex
    };

    class backend {
    public:
        backend() = default;

        bool offline = false; // dynamic val
        bool down = false; // dynamic
        int weight = 0;
        int current = 0;
        uint32_t counts = 0; // hit counter.
        int health_check_period  = 0; // msec
        int health_check_timeout = 0; // msec
        int health_check_ok_times = 0;
        std::string host;
        std::string health_check_uri;
    };

    void backend_online(backend *);
    void backend_offline(backend *);

    // smooth weighted round-robin balancing
    backend *get_backend_by_smooth_wrr();

    app::conf *cf = nullptr;
    std::atomic<int> accepted_num = {0};
    std::atomic<int> frontend_active_n = {0};
    std::atomic<int> backend_active_n = {0};
    std::atomic<int> backend_conn_ok_n = {0};
    std::atomic<int> backend_conn_fail_n = {0};
    std::mutex backend_mtx;
public:
    static int load_conf(nlohmann::json &);

    static int run_all(const ::conf *cf);

public:
    static std::unordered_map<int/*listen port*/, std::vector<app *> *> app_map_by_port;
    static std::unordered_map<std::string/*listen*/, int/*protocol*/> listen_set;
    static std::vector<app *> alls;
};

#endif // NBX_ROUTER_H_
