#ifndef NBX_HTTP_PARSER_H_
#define NBX_HTTP_PARSER_H_

class http_parser {
public:
    http_parser() = default;
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

    void reset(const char *buf, const int len) {
        this->start = buf;
        this->end = buf + len;
        this->http_major = 0;
        this->http_minor = 0;
        this->state = 0;
        this->method = http_unknown;
        this->req_start = nullptr;
        this->req_end = nullptr;
        this->uri_start = nullptr;
        this->uri_end = nullptr;
    }
    // If parsing fails, a status code will be returned, otherwise 0 will be returned
    int parse_request_line();

    // 0;
    int parse_header_line();
public:
    char http_major = 0;
    char http_minor = 0;
    char state = 0;
    char method = http_unknown;
    const char *start = nullptr;
    const char *end = nullptr;
    const char *req_start = nullptr; // request line
    const char *req_end = nullptr; // request line
    const char *uri_start = nullptr;
    const char *uri_end = nullptr;

    const char *header_name_start = nullptr;
    const char *header_name_end = nullptr;
    const char *header_start = nullptr;
    const char *header_end = nullptr;
    const char *value_start = nullptr;
    const char *value_end = nullptr;
};
#endif // NBX_HTTP_PARSER_H_
