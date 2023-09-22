#include "http_parser.h"
#include "defines.h"

// Just support METHOD /xxx?p1=x&p2=a#hash!xxx HTTP/1.0
int http_parser::parse_request_line() {
    enum {
        st_start = 0, // MUST 0
        st_method,
        st_spaces_before_uri,
        st_after_slash_in_uri,
        st_spaces_before_H,
        st_http_09,
        st_http_H, st_http_HT, st_http_HTT, st_http_HTTP,
        st_first_major_digit, // 10
        st_major_digit,
        st_first_minor_digit, // 13
        st_minor_digit,
        st_almost_done,
        st_end
    };
    const char *pos = nullptr;
    const char *space_before_uri_pos = nullptr;
    int state = st_start;
    char ch = 0;
    int mv = 0;
    for (pos = this->start; pos < this->end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_start:
            this->req_start = pos;
            if (likely(ch > ('A'-1) && ch < ('Z'-1))) { state = st_method; mv = ch; break; }
            /* strict
            if (ch == CR || ch == LF) {
                if (pos - this->req_start > MAX_EXTRA_CRLFS) return HTTP_ERR_400;
                break;
            }*/
            return HTTP_ERR_400;
        case st_method:
            if (likely(ch != ' ')) {
                mv += ch;
                if (mv > ('Z' + 'Z' + 'Z' + 'Z' + 'Z' + 'Z')) return HTTP_ERR_400;
                break;
            }
            if (mv == ('G' + 'E' + 'T')) this->method = http_get;
            else if (mv == ('P' + 'O' + 'S' + 'T')) this->method = http_post;
            else if (mv == ('P' + 'U' + 'T')) this->method = http_put;
            else if (mv == ('H' + 'E' + 'A' + 'D')) this->method = http_head;
            else if (mv == ('D' + 'E' + 'L' + 'E' + 'T' + 'E')) this->method = http_delete;
            else return HTTP_ERR_405;

            state = st_spaces_before_uri;
            space_before_uri_pos = pos;
            break;
        case st_spaces_before_uri: // not surpport METHOD http://xxx.com/sdf HTTP/1.1
            if (likely(ch == '/')) { this->uri_start = pos; state = st_after_slash_in_uri; break; }
            else if (ch == ' ') {
                if (pos - space_before_uri_pos > MAX_EXTRA_SPACES) return HTTP_ERR_400;
                break;
            }
            return HTTP_ERR_400;
        case st_after_slash_in_uri:
            if (unlikely(ch == ' ')) { this->uri_end = pos; state = st_http_09; break; }
            else if (ch == CR)  { this->uri_end = pos; this->http_minor = 9; state = st_almost_done; }
            else { if (pos - this->uri_start > MAX_URI_LEN) return HTTP_ERR_414; }
            break;
        case st_http_09:
            if (ch == 'H') { state = st_http_H; break; }
            else if (ch == ' ') { if (pos - this->uri_end > MAX_EXTRA_SPACES) return HTTP_ERR_400; }
            else if (likely(ch == CR))  {
                if (this->method != http_get)
                    return HTTP_ERR_400;
                this->http_minor = 9;
                state = st_almost_done;
            }
            break;
        case st_http_H:
            if (unlikely(ch != 'T')) return HTTP_ERR_400;
            state = st_http_HT;
            break;
        case st_http_HT:
            if (unlikely(ch != 'T')) return HTTP_ERR_400;
            state = st_http_HTT;
            break;
        case st_http_HTT:
            if (unlikely(ch != 'P')) return HTTP_ERR_400;
            state = st_http_HTTP;
            break;
        case st_http_HTTP:
            if (unlikely(ch != '/')) return HTTP_ERR_400;
            state = st_first_major_digit;
            break;
        case st_first_major_digit:
            if (unlikely(ch < '1' || ch > '9')) return HTTP_ERR_400;
            this->http_major = ch - '0';
            if (unlikely(this->http_major > 2)) return HTTP_ERR_505;
            state = st_major_digit;
            break;
        case st_major_digit:
            if (unlikely(ch != '.')) return HTTP_ERR_505;
            state = st_first_minor_digit;
            break;
        case st_first_minor_digit:
            if (unlikely(ch < '0' || ch > '9')) return HTTP_ERR_400;
            this->http_minor = ch - '0';
            state = st_minor_digit;
            break;
        case st_minor_digit:
            if (likely(ch == CR)) { state = st_almost_done; break; }
            if (unlikely(ch < '0' || ch > '9')) return HTTP_ERR_400;
            if (this->http_minor > 99) return HTTP_ERR_505;
            this->http_minor = this->http_minor * 10 + (ch - '0');
            break;
        case st_almost_done:
            if (ch == LF) { state = st_end; break; }
            return HTTP_ERR_400;
        } // end of `switch (state)'
        if (state == st_end)
            break;
    } // end of `for (int i = 0; i < len; ++i)'
    
    this->start = pos + 1;
    return state == st_end ? http_parser::parse_ok : http_parser::partial_req;
}
int http_parser::parse_uri(const char *&path_end, const char *&query_start,
    const char *&query_end) {
    enum {
        st_start = 0, // MUST 0
        st_path,
        st_query,
        st_check_fragment,
        st_end
    };
    char ch = 0;
    const char *pos = nullptr;
    int state = st_start;
    path_end = nullptr;
    query_start = nullptr;

    for (pos = this->uri_start; pos < this->uri_end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_start:
            if (unlikely(ch != '/')) { return HTTP_ERR_400; }
            state = st_path;
            break;
        case st_path:
            if (ch != '?') break;
            path_end = pos;
            state = st_query;
            break;
        case st_query:
            if (ch == '#') { state = st_end; break; }
            query_start = pos;
            state = st_check_fragment;
            break;
        case st_check_fragment:
            if (ch == '#') { query_end = pos; state = st_end; }
            break;
        }
        if (state == st_end)
            break;
    }
    if (query_start != nullptr && query_end == nullptr)
        query_end = pos + 1;
    return 0;
}
int http_parser::parse_header_line() {
    enum {
        st_start = 0, // MUST 0
        st_name,
        st_space_before_value,
        st_value,
        st_space_after_value,
        st_almost_done,
        st_req_almost_done,
        st_req_end,
        st_end
    };

    int state = st_start;
    char ch = 0;
    const char *pos = nullptr;

    for (pos = this->start; pos < this->end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_start:
            if (ch == CR) { this->req_end = pos; state = st_req_almost_done; break; }
            //if (ch == LF) { this->req_end = pos; state = st_req_end; break; }
            this->header_name_start = pos;
            this->header_start = pos;
            state = st_name;
            if (unlikely(ch <= 0x20 || ch == 0x7f || ch == ':')) { // any CHAR except CTL or SEP
                return HTTP_ERR_400;
            }
            break;
        case st_name:
            if (ch == ':') { this->header_name_end = pos; state = st_space_before_value; }
            // any CHAR except CTL or SEP
            if (unlikely(ch <= 0x20 || ch == 0x7f)) { this->req_end = pos; return HTTP_ERR_400; }
            break;
        case st_space_before_value:
            if (ch == ' ' || ch == '\t') { break; }
            else if (ch == CR) { this->req_end = pos; state = st_almost_done; }
            else if (ch == '\0') { this->req_end = pos; return HTTP_ERR_400; }
            else { this->value_start = pos; state = st_value; }
            break;
        case st_value:
            if (ch == ' ') { this->value_end = pos; state = st_space_after_value; }
            else if (ch == CR) { this->value_end = pos; state = st_almost_done; }
            break;
        case st_space_after_value:
            if (ch == ' ') { /*maybe there are spaces between value*/ break; }
            else if (ch == CR) { state = st_almost_done; break; }
            else { state = st_value; }
            break;
        case st_almost_done:
            if (ch == LF) { state = st_end; break; }
            return HTTP_ERR_400;
        case st_req_almost_done:
            if (ch == LF) { this->req_end_with_crlf = pos + 1; state = st_req_end; break; }
            return HTTP_ERR_400;
        } // end of `switch (state)'
        if (state == st_end || state == st_req_end)
            break;
    } // end of `for (int i = 0; i < len; ++i)'

    this->start = pos + 1;
    if (state == st_end) return http_parser::parse_ok;
    if (state == st_req_end) return http_parser::end_of_req;
    return http_parser::partial_req; // partial
}
// https://datatracker.ietf.org/doc/html/rfc2616#section-3.6.1
int http_parser::parse_chunked(http_parser::chunked_ret *cr) {
    enum {
        st_chunk_start = 0, // MUST 0
        st_chunk_size,
        st_chunk_extension,
        st_chunk_extension_almost_done,
        st_chunk_data,
        st_after_data,
        st_after_data_almost_done,
        st_last_chunk_extension,
        st_last_chunk_extension_almost_done,
        st_trailer,
        st_trailer_almost_done,
        st_trailer_header,
        st_trailer_header_almost_done,
        st_all_chunks_end,
        st_exit
    };
    int state = st_chunk_start;
    if (cr->result == http_parser::chunked_ret::get_chunk_data && cr->size == 0)
        state = st_after_data;
    char ch, c = 0;
    const char *pos = nullptr;
    const char *extension_start = nullptr;
    const char *trailer_headers_start = nullptr;

    for (pos = this->start; pos < this->end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_chunk_start:
            if (ch >= '0' && ch <= '9') {
                state = st_chunk_size;
                cr->size = ch - '0';
                break;
            }
            c = LOWER(ch);
            if (c >= 'a' && c <= 'f') {
                state = st_chunk_size;
                cr->size = c - 'a' + 10;
                break;
            }
            return HTTP_ERR_400;
        case st_chunk_size:
            if (cr->size > MAX_OFF_T_VALUE / 16)
                return HTTP_ERR_413;
            if (ch >= '0' && ch <= '9') {
                cr->size = cr->size * 16 + (ch - '0');
                break;
            }
            c = LOWER(ch);
            if (c >= 'a' && c <= 'f') {
                state = st_chunk_size;
                cr->size = cr->size * 16 + (c - 'a' + 10);
                break;
            }
            if (cr->size == 0) { // last-chunk line
                if (ch == CR) { state = st_last_chunk_extension_almost_done; break; }
                if (ch == ';') { state = st_last_chunk_extension; break; }
                return HTTP_ERR_400;
            }
            if (ch == CR) { state = st_chunk_extension_almost_done; break; }
            if (ch == ';') { state = st_chunk_extension; break; }
            return HTTP_ERR_400;
        case st_chunk_extension:
            if (extension_start == nullptr) { extension_start = pos; }
            else if (pos - extension_start > MAX_EXTENSION_LEN_IN_CHUNK) { return HTTP_ERR_400;}
            if (ch == CR) { state = st_chunk_extension_almost_done; break; }
            break;
        case st_chunk_extension_almost_done:
            if (ch == LF) { state = st_chunk_data; break; }
            return HTTP_ERR_400;
        case st_last_chunk_extension:
            if (extension_start == nullptr) { extension_start = pos; }
            else if (pos - extension_start > MAX_EXTENSION_LEN_IN_CHUNK) { return HTTP_ERR_400;}
            if (ch == CR) { state = st_last_chunk_extension_almost_done; break; }
            break;
        case st_last_chunk_extension_almost_done:
            if (ch == LF) { state = st_trailer; break; }
            return HTTP_ERR_400;
        case st_chunk_data:
            cr->data_start = pos;
            break;
        case st_after_data:
            if (ch == CR) { state = st_after_data_almost_done; break; }
            return HTTP_ERR_400;
        case st_after_data_almost_done:
            if (ch == LF) { state = st_chunk_start; break; }
            return HTTP_ERR_400;
        case st_trailer:
            if (ch == CR) { state = st_trailer_almost_done; break; }
            state = st_trailer_header;
            if (trailer_headers_start == nullptr) trailer_headers_start = pos;
            else if (pos - trailer_headers_start > MAX_TRAILER_HEADERS_LEN_IN_CHUNK)
                return HTTP_ERR_400;
            break;
        case st_trailer_almost_done:
            if (ch == LF) { state = st_all_chunks_end; break; }
            return HTTP_ERR_400;
        case st_trailer_header:
            if (ch == CR) { state = st_trailer_header_almost_done; break; }
            break;
        case st_trailer_header_almost_done:
            if (ch == LF) { state = st_trailer; break; }
            return HTTP_ERR_400;
        }
        if (state == st_chunk_data || state == st_all_chunks_end)
            break;
    }
    if (pos > this->start && pos != this->end)
        this->start = pos + 1;
    cr->result = http_parser::chunked_ret::partial_chunk;
    if (state == st_all_chunks_end)
        cr->result = http_parser::chunked_ret::all_chunk_end;
    else if (state == st_chunk_data)
        cr->result = http_parser::chunked_ret::get_chunk_data;
    return 0;
}
