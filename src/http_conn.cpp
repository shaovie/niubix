#include "http_conn.h"
#include "app.h"
#include "log.h"
#include "socket.h"
#include "backend_conn.h"
#include "worker.h"
#include "acceptor.h"
#include "connector.h"
#include "defines.h"
#include "inet_addr.h"

#include <string.h>

http_conn::~http_conn() {
    if (this->sockaddr != nullptr)
        ::free(this->sockaddr);
    if (this->backend != nullptr)
        this->backend->on_close();
    if (this->partial_buf != nullptr)
        ::free(this->partial_buf);
    if (this->local_addr != nullptr)
        ::free(this->local_addr);
    if (this->remote_addr != nullptr)
        ::free(this->remote_addr);
}
void http_conn::set_remote_addr(const struct sockaddr *addr, const socklen_t /*socklen*/) {
    //this->sockaddr = (struct sockaddr *)::malloc(socklen); // TODO optimize
    //::memcpy(this->sockaddr, addr, socklen);
    if (this->remote_addr == nullptr)
        this->remote_addr = (char *)::malloc(INET6_ADDRSTRLEN);
    this->remote_addr[INET6_ADDRSTRLEN-1] = '\0';
    if (socket::addr_to_string(addr, this->remote_addr, INET6_ADDRSTRLEN) == 0)
        this->remote_addr_len = ::strlen(this->remote_addr);
}
bool http_conn::on_open() {
    this->start_time = this->wrker->now_msec;

    if (this->local_addr == nullptr)
        this->local_addr = (char *)::malloc(INET6_ADDRSTRLEN);
    this->local_addr[INET6_ADDRSTRLEN-1] = '\0';
    if (socket::get_local_addr(this->get_fd(), this->local_addr, INET6_ADDRSTRLEN) != 0) {
        log::error("new conn get local addr fail %s", strerror(errno));
        return false;
    }
    this->local_addr_len = ::strlen(this->local_addr);
    auto app_l = app::app_map_by_port.find(this->acc->port);
    if (unlikely(app_l == app::app_map_by_port.end())) {
        log::error("new conn not match app by local port%d", this->acc->port);
        return false;
    }
    auto vp = app_l->second;
    if (vp->size() == 1) { // 该端口只绑定了一个app, 立即准备连接后端
        this->matched_app = vp->front();
        if (this->to_connect_backend() != 0)
            return false;
    }
    return true;
}
int http_conn::to_connect_backend() {
    int accepted_num = this->matched_app->accepted_num.fetch_add(1, std::memory_order_relaxed);
    // route
    if (this->matched_app->cf->policy == app::roundrobin) {
        int idx = accepted_num % this->matched_app->cf->backend_list.size();
        app::backend *ab = this->matched_app->cf->backend_list[idx];
        struct sockaddr_in taddr;
        inet_addr::parse_v4_addr(ab->host, &taddr);
        nbx_inet_addr naddr{(struct sockaddr*)&taddr, sizeof(taddr)};
        this->backend = new backend_conn(this->wrker, this);
        if (this->wrker->conn->connect(this->backend, naddr,
                this->matched_app->cf->connect_backend_timeout) == -1) {
            log::warn("connect to backend:%s fail!", ab->host.c_str());
            return -1;
        }
        return 0;
    }
    return -1;
}
int http_conn::on_backend_connect_ok() {
    int fd = this->get_fd();
    socket::set_nodelay(fd);

    if (this->wrker->add_ev(this, fd, ev_handler::ev_read) != 0) {
        log::error("new http_conn add to poller fail! %s", strerror(errno));
        this->backend = nullptr;
        this->on_close();
        return -1;
    }
    return 0;
}
void http_conn::on_backend_connect_fail(const int /*err*/) {
    if (this->backend != nullptr)
        this->backend = nullptr;
    this->on_close();
}
void http_conn::on_backend_close() {
    if (this->backend != nullptr)
        this->backend = nullptr;

    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void http_conn::on_close() {
    if (this->backend != nullptr) {
        this->backend->on_frontend_close();
        this->backend = nullptr;
    }
    this->wrker->cancel_timer(this);
    this->destroy();
    delete this;
}
bool http_conn::on_read() {
    char *buf = nullptr;
    int ret = this->recv(buf);
    if (ret == 0) // closed
        return false;
    else if (ret < 0)
        return true;
    
    buf[ret] = '\0';
    return this->handle_request(buf, ret);
}
#define save_partial_buf() if (this->partial_buf_len == 0) { \
                               this->partial_buf = (char*)::malloc(len); \
                               ::memcpy(this->partial_buf, buf, len - buf_offset); \
                               this->partial_buf_len = len; \
                           }
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

bool http_conn::handle_request(const char *rbuf, int rlen) {
    if (this->partial_buf_len > 0) {
        char *tbuf = (char*)::malloc(this->partial_buf_len + rlen);
        ::memcpy(tbuf, this->partial_buf, this->partial_buf_len);
        ::memcpy(tbuf + this->partial_buf_len, rbuf, rlen);
        ::free(this->partial_buf);
        this->partial_buf = tbuf;
        this->partial_buf_len += rlen;
        rbuf = this->partial_buf;
        rlen = this->partial_buf_len;
    }
    int buf_offset = 0;
    int len = rlen;
    do {
        const char *buf = rbuf + buf_offset;
        len -= buf_offset;
        if (len < 1)
            break;
        buf_offset = 0;
        if (len - buf_offset < (int)sizeof("GET / HTTP/1.x\r\n\r\n") - 1) { // Shortest Request
            save_partial_buf();
            return true; // partial
        }
        // 1. METHOD  GET/POST
        int method = 0; // get:1
        const char *xff_start = nullptr;
        int xff_len = 0;
        if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
            if (buf[3] != ' ')
                return false; // invalid
            method = 1;
            buf_offset += 4;
        } else if (buf[0] == 'P' && buf[1] == 'O' && buf[3] == 'T') {
            if (buf[4] != ' ')
                return false; // invalid
            method = 2;
            buf_offset += 5;
        }
        if (method == 0)
            return false; // unsurpported

        // 2. URI /a/b/c?p=x&p2=2#yyy 
        //    URI http://xx.com/a/b/c?p=x&p2=2#yyy 
        for (buf_offset += 1/*skip -1*/; buf_offset < len; ++buf_offset) {
            if (buf[buf_offset-1] == '\r' && buf[buf_offset] == '\n')
                break;
        }
        if (buf_offset >= len) { // header line partial
            save_partial_buf();
            return true;
        }
        int header_line_end = buf_offset + 1;
        if (header_line_end > 4095) // too long
            return false;

        ++buf_offset;

        if (len - buf_offset < 2) { // no \r\n ?
            save_partial_buf();
            return true;
        }
        if (buf[buf_offset] == '\r' && buf[buf_offset+1] == '\n') {
            // GET / HTTP/1.x\r\n\r\n
            this->a_complete_request(buf, buf_offset+1+1, header_line_end, nullptr, 0);
            return true;
        }

        // 3 header fileds key:value\r\n
        for(; buf_offset < len;) {
            const char *start = buf + buf_offset;
            if (*start == '\r')
                break;
            char *p = (char *)::memchr(start, '\n', len - buf_offset);
            if (p == nullptr) { // partial
                save_partial_buf();
                return true;
            }
            if (p - start + 1 >= (int)sizeof("X-Forwarded-For:0.0.0.0" - 1)
                && ::strncasecmp(start, "X-Forwarded-For:", sizeof("X-Forwarded-For:" - 1)) == 0) {
                xff_start = start;
                xff_len = p - 1 - start;
                break;
            }
            buf_offset += p - start + 1;
        }

        if (len - buf_offset < 2) { // no end of \r\n ?
            save_partial_buf();
            return true;
        }
        if (buf[buf_offset] == '\r' && buf[buf_offset+1] == '\n')
            this->a_complete_request(buf, buf_offset+1+1, header_line_end, xff_start, xff_len);

        buf_offset += 2;
    } while (true);
    return true;
        /*
end:
buf_offset += 2;
if (buf_offset != len) {
log::error("Unknown request %s", buf); // only support ping/pong mode, not support pipeling
return false;
}
return true;
*/
}
int http_conn::a_complete_request(const char *buf, const int len,
    const int header_line_end,
    const char *xff_start,
    const int xff_len) {

    char tbuf[4096];
    ::memcpy(tbuf, buf, header_line_end);
    int copy_len = header_line_end;
    ::memcpy(tbuf + copy_len, "X-Real-IP: ", sizeof("X-Real-IP: ") - 1);
    copy_len += sizeof("X-Real-IP: ") - 1;
    ::memcpy(tbuf + copy_len, this->remote_addr, this->remote_addr_len);
    copy_len += this->remote_addr_len;
    ::memcpy(tbuf + copy_len, "\r\n", 2);
    copy_len += 2;

    if (xff_start == nullptr) { // add
        ::memcpy(tbuf + copy_len, "X-Forwarded-For: ", sizeof("X-Forwarded-For: ") - 1);
        copy_len += sizeof("X-Forwarded-For: ") - 1;
    } else {
        ::memcpy(tbuf + copy_len, xff_start, xff_len);
        copy_len += xff_len - 2/*CRLF*/;
        ::memcpy(tbuf + copy_len, ", ", 2);
        copy_len += 2;
    }
    ::memcpy(tbuf + copy_len, this->local_addr, this->local_addr_len);
    copy_len += this->local_addr_len;
    ::memcpy(tbuf + copy_len, "\r\n", 2);
    copy_len += 2;
    tbuf[copy_len] = '\0';
    this->backend->send(tbuf, copy_len);
    this->backend->send(buf + header_line_end, len - header_line_end);
    return true;
}
