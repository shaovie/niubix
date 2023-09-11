#ifndef NBX_INET_ADDR_H_
#define NBX_INET_ADDR_H_

#include <string>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/socket.h>

typedef union {
    struct sockaddr           sockaddr;
    struct sockaddr_in        sockaddr_in;
    struct sockaddr_in6       sockaddr_in6;
    struct sockaddr_un        sockaddr_un;
} nbx_sockaddr_t;

struct nbx_inet_addr {
    struct sockaddr *addr;
    socklen_t sock_len;
};

class inet_addr {
public:
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

    // addr "192.168.0.1:8080"
    static int parse_v4_addr(const std::string &addr, struct sockaddr_in *saddr) {
        auto p = addr.find(":");
        if (p == addr.npos || (p > 0 && p < 7))
            return -1;
        std::string ip = addr.substr(0, p);
        int port = 0;
        try {
            port = std::stoi(addr.substr(p+1, addr.length() - p - 1)); // throw
        } catch(...) {
            return -1;
        }
        if (ip.empty() || port < 1 || port > 65535)
            return -1;
        saddr->sin_family = AF_INET;
        saddr->sin_port = ::htons(port);
        ::inet_pton(AF_INET, ip.c_str(), &(saddr->sin_addr));
        ::memset(saddr->sin_zero, 0, sizeof(saddr->sin_zero));
        return 0;
    }
};

#endif // NBX_INET_ADDR_H_
