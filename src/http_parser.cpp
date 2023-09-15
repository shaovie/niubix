#include "http_parser.h"
#include "defines.h"
#include "http.h"
#include <stdio.h>

/* Tokens as defined by rfc 2616. Also lowercases them.
 *        token       = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */
static const char tokens[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel */
    0,       0,       0,       0,       0,       0,       0,       0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si  */
    0,       0,       0,       0,       0,       0,       0,       0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0,       0,       0,       0,       0,       0,       0,       0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0,       0,       0,       0,       0,       0,       0,       0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    ' ',     '!',      0,      '#',     '$',     '%',     '&',    '\'',
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    0,       0,      '*',     '+',      0,      '-',     '.',      0,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    '0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    '8',     '9',      0,       0,       0,       0,       0,       0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    0,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    'x',     'y',     'z',      0,       0,       0,      '^',     '_',
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    '`',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    'x',     'y',     'z',      0,      '|',      0,      '~',       0 };

// not surpport HTTP/0.9
// Just support METHOD /xxx?p1=x&p2=a#hash!xxx HTTP/1.0
int http_parser::parse_request_line() {
    enum {
        st_start = 0, // MUST 0
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
    const char *pos = nullptr;
    const char *space_before_uri_pos = nullptr;
    const char *space_after_version_pos = nullptr;
    int state = this->state;
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
            else { if (pos - this->uri_start > MAX_URI_LEN) return 414; }
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
    return state == st_end ? 0 : -1;
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

    int state = this->state;
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
            else if (ch == LF) { this->header_end = pos; state = st_end; }
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
    if (state == st_end) return 0;
    if (state == st_header_end) return -2;
    return -1; // partial
}
