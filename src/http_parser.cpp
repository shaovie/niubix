#include "http_parser.h"
#include "defines.h"
#include <stdio.h>

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
        st_first_major_digit,
        st_after_version,
        st_major_digit,
        st_first_minor_digit,
        st_minor_digit,
        st_spaces_after_version,
        st_almost_done,
        st_end
    };
    const char *pos = nullptr;
    const char *space_before_uri_pos = nullptr;
    const char *space_after_version_pos = nullptr;
    int state = st_start;
    char ch = 0;
    int mv = 0;
    for (pos = this->start; pos < this->end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_start:
            this->req_start = pos;
            if (likely(ch > ('A'-1) && ch < ('Z'-1))) { state = st_method; mv = ch; break; }
            if (ch == CR || ch == LF) {
                if (pos - this->req_start > MAX_EXTRA_CRLFS) return HTTP_ERR_400;
                break;
            }
            return HTTP_ERR_400;
        case st_method:
            if (likely(ch != ' ')) {
                mv += ch;
                if (mv > 'Z' + 'Z' + 'Z' + 'Z' + 'Z' + 'Z') return HTTP_ERR_400;
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
            else if (ch == LF)  { this->uri_end = pos; this->http_minor = 9; state = st_end; }
            else { if (pos - this->uri_start > MAX_URI_LEN) return HTTP_ERR_414; }
            break;
        case st_http_09:
            if (ch == 'H') { state = st_http_H; break; }
            else if (ch == ' ') { if (pos - this->uri_end > MAX_EXTRA_SPACES) return HTTP_ERR_400; }
            else if (likely(ch == CR))  { this->http_minor = 9; state = st_almost_done; }
            else if (ch == LF)  { this->http_minor = 9; state = st_end; }
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
            if (unlikely(this->http_major > 1)) return HTTP_ERR_400;
            state = st_major_digit;
            break;
        case st_major_digit: // 只支持 1.0 1.1  1位版本号, 不支持2位及以上, e.g. 10.1, 1.12
            if (unlikely(ch != '.')) return HTTP_ERR_400;
            state = st_first_minor_digit;
            break;
        case st_first_minor_digit:
            if (unlikely(ch < '1' || ch > '9')) return HTTP_ERR_400;
            this->http_minor = ch - '0';
            state = st_after_version;
            break;
        case st_after_version:
            if (likely(ch == CR)) { state = st_almost_done; break; }
            if (ch != ' ') return HTTP_ERR_400;
            space_after_version_pos = pos;
            state = st_spaces_after_version;
            break;
        case st_spaces_after_version:
            if (likely(ch == CR)) { state = st_almost_done; break; }
            if (unlikely(ch == ' ')) {
                if (pos - space_after_version_pos + 1 > MAX_EXTRA_SPACES)
                    return HTTP_ERR_400;
                break;
            }
            if (ch == LF) { state = st_end; break; }
            return HTTP_ERR_400;
        case st_almost_done:
            this->req_end = pos - 1;
            if (ch == LF) { state = st_end; break; }
            return HTTP_ERR_400;
        } // end of `switch (state)'
        if (state == st_end)
            break;
    } // end of `for (int i = 0; i < len; ++i)'
    
    this->start = pos + 1;
    if (this->req_end == nullptr)
        this->req_end = pos;
    return state == st_end ? http_parser::parse_ok : http_parser::partial_req;
}
int http_parser::parse_header_line() {
    enum {
        st_start = 0, // MUST 0
        st_name,
        st_space_before_value,
        st_value,
        st_space_after_value,
        st_ignore_line,
        st_almost_done,
        st_header_almost_done,
        st_header_end,
        st_end
    };

    int state = st_start;
    char ch = 0;
    const char *pos = nullptr;

    for (pos = this->start; pos < this->end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_start:
            if (ch == CR) { this->header_end = pos; state = st_header_almost_done; break; }
            if (ch == LF) { this->header_end = pos; state = st_header_end; break; }
            this->header_name_start = pos;
            this->header_start = pos;
            state = st_name;
            if (unlikely(ch <= 0x20 || ch == 0x7f || ch == ':')) { // any CHAR except CTL or SEP
                this->header_end = pos;
                return HTTP_ERR_400;
            }
            break;
        case st_name:
            if (ch == ':') { this->header_name_end = pos; state = st_space_before_value; }
            else if (ch == CR) { this->header_name_end = pos; this->header_end = pos; state = st_almost_done;}
            else if (ch == LF) { this->header_name_end = pos; this->header_end = pos; state = st_end; }
            // any CHAR except CTL or SEP
            if (unlikely(ch <= 0x20 || ch == 0x7f)) { this->header_end = pos; return HTTP_ERR_400; }
            break;
        case st_space_before_value:
            if (ch == ' ') {  break; }
            else if (ch == CR) { this->header_end = pos; state = st_almost_done; }
            else if (ch == LF) { this->header_end = pos; state = st_end; }
            else if (ch == '\0') { this->header_end = pos; return HTTP_ERR_400; }
            else { this->value_start = pos; state = st_value; }
            break;
        case st_value:
            if (ch == ' ') { this->header_end = pos; this->value_end = pos; state = st_space_after_value; }
            else if (ch == CR) { this->header_end = pos; this->value_end = pos; state = st_almost_done; }
            else if (ch == LF) { this->header_end = pos; this->value_end = pos; state = st_end; }
            else if (ch == '\0') { this->header_end = pos; return HTTP_ERR_400; }
            break;
        case st_space_after_value:
            if (ch == ' ') { /*maybe there are spaces between value*/ break; }
            else if (ch == CR) { state = st_almost_done; break; }
            else if (ch == LF) { state = st_end; break; }
            else if (ch == '\0') { this->header_end = pos; return HTTP_ERR_400; }
            else { state = st_value; }
            break;
        case st_almost_done:
            if (ch == LF) { state = st_end; break; }
            if (ch == CR) { break; }
            return HTTP_ERR_400;
        case st_header_almost_done:
            if (ch == LF) { state = st_header_end; break; }
            return HTTP_ERR_400;
        } // end of `switch (state)'
        if (state == st_end || state == st_header_end)
            break;
    } // end of `for (int i = 0; i < len; ++i)'

    this->start = pos + 1;
    if (state == st_end) return http_parser::parse_ok;
    if (state == st_header_end) return http_parser::eof;
    return http_parser::partial_req; // partial
}
