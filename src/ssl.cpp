#include "ssl.h"
#include "log.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>

// openssl version >= 1.0.2

SSL_CTX *ssl::ctx = nullptr;

int ssl::init() {
#if OPENSSL_VERSION_NUMBER >= 0x1010003fL
    auto init = OPENSSL_INIT_new();
    if (init == nullptr) {
        log::error("OPENSSL_INIT_new fail!");
        return -1;
    }
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, init) == 0) {
        log::error("OPENSSL_init_ssl fail!");
        return -1;
    }
    OPENSSL_INIT_free(init);
    ERR_clear_error();
#else
    OPENSSL_config(nullptr);
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif

    ssl::ctx = ssl::create_ctx();
    if (ssl::ctx == nullptr)
        return -1;
    return 0;
}
SSL_CTX *ssl::create_ctx() {
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
    if (ctx == nullptr) {
        log::error("SSL_CTX_new fail!");
        return nullptr;
    }

    // copied from nginx src/event/ngx_event_openssl.c
    /* client side options*/
    SSL_CTX_set_options(ctx, SSL_OP_MICROSOFT_SESS_ID_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_NETSCAPE_CHALLENGE_BUG);

    /* server side options*/
    SSL_CTX_set_options(ctx, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);

    SSL_CTX_set_options(ctx, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_TLS_D5_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_TLS_BLOCK_PADDING_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_ANTI_REPLAY);
    SSL_CTX_set_options(ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
    SSL_CTX_set_min_proto_version(ctx, 0);
    SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_min_proto_version(ctx, 0);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
    SSL_CTX_set_mode(ctx, SSL_MODE_NO_AUTO_CHAIN);

    SSL_CTX_clear_options(ctx, SSL_OP_NO_TLSv1_1);
    /* only in 0.9.8m+ */
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1);

    SSL_CTX_set_read_ahead(ctx, 1);

    // handshake 过程会回调通过状态变化
    //SSL_CTX_set_info_callback(ctx, ngx_ssl_info_callback);
    return ctx;
}
SSL *ssl::on_accepted(const int fd, SSL_CTX *ctx) {
    SSL *conn = SSL_new(ctx);
    if (conn == nullptr) {
        log::error("SSL_new fail!");
        return nullptr;
    }
    if (SSL_set_fd(conn, fd) == 0) {
        log::error("SSL_set_fd fail!");
        return nullptr;
    }
    SSL_set_accept_state(conn);
    SSL_set_options(conn, SSL_OP_NO_RENEGOTIATION);

    return conn;
}
