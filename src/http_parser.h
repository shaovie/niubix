#ifndef NBX_HTTP_CONN_H_
#define NBX_HTTP_CONN_H_

class http_parser {
public:
    http_parser() = default;
    enum {
        st_start = 0,
        st_method,
        st_spaces_before_uri,
        st_schema,              // x
        st_schema_slash,        // x
        st_schema_slash_slash,  // x
        st_host_start,          // x
        st_host,                // x
        st_host_end,            // x
        st_host_ip_literal,     // x
        st_port,                // x
        st_after_slash_in_uri,
        st_check_uri,
        st_uri,
        st_spaces_before_H,
        st_http_09,
        st_http_H,
        st_http_HT,
        st_http_HTT,
        st_http_HTTP,
        st_first_major_digit,
        st_after_version,
        st_major_digit,
        st_first_minor_digit,
        st_minor_digit,
        st_spaces_after_version,
        st_almost_done,
        st_end
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
    };

    void reset(const char *buf, const int len) {
        this->start = buf;
        this->end = buf + len;
        this->http_major = 0;
        this->http_minor = 0;
        this->state = st_start;
        this->method = http_unknown;
        this->req_start = nullptr;
        this->req_end = nullptr;
        this->method_start = nullptr;
        this->uri_start = nullptr;
        this->uri_end = nullptr;
    }
    //
    int parse_request_line();
    int parse_request_line2();
public:
    char http_major = 0;
    char http_minor = 0;
    char state = st_start;
    char method = http_unknown;
    const char *start = nullptr;
    const char *end = nullptr;
    const char *req_start = nullptr;
    const char *req_end = nullptr;
    const char *method_start = nullptr;
    const char *uri_start = nullptr;
    const char *uri_end = nullptr;
};
#endif // NBX_HTTP_CONN_H_
