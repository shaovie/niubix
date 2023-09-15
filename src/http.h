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
    HTTP_ERR_414,
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
enum {
    http_v_9     = 9,
    http_v_10    = 10,
    http_v_11    = 11,
    http_v_20    = 20,
};
enum {
    http_unknown = 0,
    http_get     = 1,
    http_post    = 2,
    http_put     = 3,
    http_delete  = 4,
    http_head    = 5,

    http_method_max_len = 6, // delete
};

class http {
public:
    static const int   err_codes[HTTP_ERR_SIZE];
    static const char *err_msgs[HTTP_ERR_SIZE];
    static const char *get_reason(const int status);
};

#endif // NBX_HTTP_H_
