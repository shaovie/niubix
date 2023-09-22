#ifndef NBX_HTTP_PARSER_H_
#define NBX_HTTP_PARSER_H_

#include "http.h"

class http_parser {
public:
    enum {
        partial_req     = 9001,
        parse_ok        = 9002,
        end_of_req      = 9003,
    };
    class chunked_ret {
    public:
        enum {
            partial_chunk   = 1, // chunk struct incomplete
            get_chunk_data  = 2,
            all_chunk_end   = 3,
        };
        chunked_ret() = default;
        int result = 0;
        uint64_t size = 0; // chunk data size, not included CRLF
        const char *data_start = nullptr;
    };
    http_parser() = default;
    http_parser(const char *start, const char *end): start(start), end(end) { }
    inline void reset(const char *start, const char *end) {
        this->start = start;
        this->end = end;
    }

    inline int http_version() { return (int)this->http_major * 100 + (int)this->http_minor; }

    // If parsing fails, a status code will be returned, otherwise 0 will be returned
    int parse_request_line();
    int parse_uri(const char *&path_end, const char *&query_start, const char *&query_end);

    int parse_header_line();
    int parse_chunked(http_parser::chunked_ret *cr);
public:
    char http_major                 = 0;
    char http_minor                 = 0;
    char method                     = http_unknown;

    const char *start               = nullptr;
    const char *end                 = nullptr;

    const char *req_start           = nullptr; // request line
    const char *req_end             = nullptr; // not containing empty lines \r\n
    const char *req_end_with_crlf   = nullptr; // include \r\n
    const char *uri_start           = nullptr;
    const char *uri_end             = nullptr;

    const char *header_name_start   = nullptr;
    const char *header_name_end     = nullptr;
    const char *header_start        = nullptr;
    const char *value_start         = nullptr;
    const char *value_end           = nullptr;
};
#endif // NBX_HTTP_PARSER_H_
