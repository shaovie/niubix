#ifndef NBX_HTTP_PARSER_H_
#define NBX_HTTP_PARSER_H_

#include "http.h"

class http_parser {
public:
    enum {
        partial_req     = 9001,
        parse_ok        = 9002,
        eof             = 9003,
    };
    http_parser() = default;
    http_parser(const char *start, const char *end): start(start), end(end) { }
    inline void reset(const char *start, const char *end) {
        this->start = start;
        this->end = end;
    }

    // If parsing fails, a status code will be returned, otherwise 0 will be returned
    int parse_request_line();

    // 0;
    int parse_header_line();
public:
    char http_major                 = 0;
    char http_minor                 = 0;
    char method                     = http_unknown;

    const char *start               = nullptr;
    const char *end                 = nullptr;

    const char *req_start           = nullptr; // request line
    const char *req_end             = nullptr; // not containing empty lines \r\n
    const char *uri_start           = nullptr;
    const char *uri_end             = nullptr;

    const char *header_name_start   = nullptr;
    const char *header_name_end     = nullptr;
    const char *header_start        = nullptr;
    const char *header_end          = nullptr;
    const char *value_start         = nullptr;
    const char *value_end           = nullptr;
};
#endif // NBX_HTTP_PARSER_H_
