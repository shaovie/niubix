#ifndef NBX_HTTP_H_
#define NBX_HTTP_H_

#define MAX_FULL_REQ_LEN    8192
#define MAX_URI_LEN         2048
#define MAX_EXTRA_CRLFS     16  // "\r\n" = 2
#define MAX_EXTRA_SPACES    8

/* All implemented HTTP status codes */
enum {
    HTTP_ERR_200 = 0,
    HTTP_ERR_400,
    HTTP_ERR_401,
    HTTP_ERR_403,
    HTTP_ERR_404,
    HTTP_ERR_405,
    HTTP_ERR_407,
    HTTP_ERR_408,
    HTTP_ERR_410,
    HTTP_ERR_413,
    HTTP_ERR_421,
    HTTP_ERR_422,
    HTTP_ERR_425,
    HTTP_ERR_429,
    HTTP_ERR_500,
    HTTP_ERR_501,
    HTTP_ERR_502,
    HTTP_ERR_503,
    HTTP_ERR_504,
    HTTP_ERR_SIZE
};
class http {
public:
    static const int   err_codes[HTTP_ERR_SIZE];
    static const char *err_msgs[HTTP_ERR_SIZE];
    static const char *get_reason(const int status);
};

#endif // NBX_HTTP_H_
