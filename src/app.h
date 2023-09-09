#ifndef NBX_ROUTER_H_
#define NBX_ROUTER_H_

#include "nlohmann/json_fwd.hpp"

#include <map>
#include <string>
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
    // addr ipv4: "192.168.0.1:8080" or ":8080"
    // addr ipv6: "[2001:470:1f18:471::2]:8080" or "[]:8080"
    static int parse_ip_port(const std::string &addr, std::string &ip, int &port) {
        if (addr.length() < 2)
            return -1;
        if (addr[0] == '[') { // ipv6
            auto p = addr.rfind(":");
            if (p == addr.npos || (p > 0 && p < 2))
                return -1;
            ip = addr.substr(1, p - 1);
            try {
                port = std::stoi(addr.substr(p+1, addr.length() - p - 1)); // throw
            } catch(...) {
                return -1;
            }
        } else {
            auto p = addr.find(":");
            if (p == addr.npos || (p > 0 && p < 7))
                return -1;
            ip = addr.substr(0, p);
            try {
                port = std::stoi(addr.substr(p+1, addr.length() - p - 1)); // throw
            } catch(...) {
                return -1;
            }
        }
        return 0;
    }

    class backend;
    class conf {
    public:
        int policy = 0;
        int connect_backend_timeout = 1000; // msec
        int health_check_timeout    = 1000; // msec
        int protocol;
        std::string listen;
        std::string host;
        std::string health_check_uri;
        std::vector<backend *> backend_list;
    };

    class backend {
    public:
        backend() = default;

        int weight = 0;
        std::string host;
    };

public:
    static int load_conf(nlohmann::json &);

    static int run_all(const ::conf *cf);

public:
    static std::map<std::string, int/*protocol*/> listen_set;
    static std::unordered_map<std::string/*host*/, app::conf *> app_map;
};

#endif // NBX_ROUTER_H_
