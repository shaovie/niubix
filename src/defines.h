#ifndef NBX_DEFINES_H_
#define NBX_DEFINES_H_

#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define likely(x)   __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

//= log
#define MAX_LENGTH_OF_ONE_LOG                4095 
#define MAX_FILE_NAME_LENGTH                 255

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

#endif // NBX_DEFINES_H_
