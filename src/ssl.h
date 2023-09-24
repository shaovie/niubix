#ifndef NBX_SSL_H_
#define NBX_SSL_H_

#include <openssl/ssl.h>

class ssl {
public:
    static int init();
    static SSL_CTX *create_ctx();
    static SSL *on_accepted(const int fd, SSL_CTX *ctx);

public:
    static SSL_CTX *ctx;
};

#endif // NBX_SSL_H_
