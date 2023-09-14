#include "http_parser.h"
#include "defines.h"

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
    int state = this->state;
    const char *pos = nullptr;
    const char *m = nullptr;
    char ch = 0;
    for (pos = this->start; pos < this->end; ++pos) {
        ch = *pos;
        switch (state) {
        case st_start:
            this->req_start = pos;
            if (ch == CR || ch == LF) break;
            if (ch < 'A' || ch > 'Z') return -1;
            state = st_method;
            this->method_start = pos;
            --pos;
            break;
        case st_method:
            if (ch != ' ') break;
            m = this->method_start;
            ch = *m;
            switch (pos - m) {
            case 3:
                if (ch == 'G' && m[1] == 'E' && m[2] == 'T')
                    this->method = http_get;
                else if (ch == 'P' && m[1] == 'U' && m[2] == 'T')
                    this->method = http_put;
                break;
            case 4:
                if (ch == 'P' && m[1] == 'O' && m[2] == 'S' && m[3] == 'T')
                    this->method = http_post;
                break;
            case 6:
                if (ch == 'D' && m[1] == 'E' && m[2] == 'L' && m[4] == 'T')
                    this->method = http_delete;
                break;
            } // end of `switch (pos - m)'

            if (this->method == http_unknown) return -1;
            state = st_spaces_before_uri;
            break;
        case st_spaces_before_uri:
            if (ch == '/') {
                this->uri_start = pos;
                state = st_after_slash_in_uri;
                break;
            } else if (ch != ' ') // not surpport METHOD http://xxx.com/sdf HTTP/1.1
                return -1;
            break;
        case st_after_slash_in_uri:
            if (ch == ' ') { this->uri_end = pos; state = st_http_09; }
            if (ch == CR)  { this->uri_end = pos; this->http_minor = 9; state = st_almost_done; }
            if (ch == LF)  { this->uri_end = pos; this->http_minor = 9; state = st_end; }
            break;
        case st_http_09:
            if (ch == ' ') break;
            if (ch == CR)  { this->http_minor = 9; state = st_almost_done; }
            if (ch == LF)  { this->http_minor = 9; state = st_end; }
            if (ch == 'H') { state = st_http_H; };
            break;
        case st_http_H:
            if (ch != 'T') return -1;
            state = st_http_HT;
            break;
        case st_http_HT:
            if (ch != 'T') return -1;
            state = st_http_HTT;
            break;
        case st_http_HTT:
            if (ch != 'P') return -1;
            state = st_http_HTTP;
            break;
        case st_http_HTTP:
            if (ch != '/') return -1;
            state = st_first_major_digit;
            break;
        case st_first_major_digit:
            if (ch < '1' || ch > '9') return -1;
            this->http_major = ch - '0';
            if (this->http_major > 1) return -1;
            state = st_major_digit;
            break;
        case st_major_digit: // 只支持 1.0 1.1  版本号1位数, 不支持10.1, 1.12
            if (ch != '.') return -1;
            state = st_first_minor_digit;
            break;
        case st_first_minor_digit:
            if (ch < '1' || ch > '9') return -1;
            this->http_minor = ch - '0';
            state = st_after_version;
            break;
        case st_after_version:
            if (ch == CR) { state = st_almost_done; break; }
            if (ch != ' ') return -1;
            state = st_spaces_after_version;
            break;
        case st_spaces_after_version:
            if (ch == ' ') break;
            if (ch == CR) { state = st_almost_done; break; }
            if (ch == LF) { state = st_end; break; }
            return -1;
        case st_almost_done:
            this->req_end = pos - 1;
            if (ch == LF) { state = st_end; break; }
            return -1;
        } // end of `switch (state)'
        if (state == st_end)
            break;
    } // end of `for (int i = 0; i < len; ++i)'
    this->state = st_start;
    this->start = pos + 1;
    if (this->req_end == nullptr)
        this->req_end = pos;
    return 0;
}
