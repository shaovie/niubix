#include "http_frontend.h"
#include "app.h"
#include "log.h"
#include "socket.h"
#include "backend.h"
#include "worker.h"
#include "acceptor.h"
#include "connector.h"
#include "defines.h"
#include "inet_addr.h"
#include "http_parser.h"

#include <random>
#include <cstring>

http_frontend::~http_frontend() {
    if (this->partial_buf != nullptr)
        ::free(this->partial_buf);
    if (this->local_addr != nullptr)
        ::free(this->local_addr);
    if (this->remote_addr != nullptr)
        ::free(this->remote_addr);
}
void http_frontend::set_remote_addr(const struct sockaddr *addr, const socklen_t /*socklen*/) {
    if (this->remote_addr == nullptr)
        this->remote_addr = (char *)::malloc(INET6_ADDRSTRLEN); // TODO optimize
    this->remote_addr[INET6_ADDRSTRLEN-1] = '\0';
    if (socket::addr_to_string(addr, this->remote_addr, INET6_ADDRSTRLEN) == 0)
        this->remote_addr_len = ::strlen(this->remote_addr);
}
bool http_frontend::on_open() {
    this->start_time = this->wrker->now_msec;
    this->state = conn_ok;

    if (this->local_addr == nullptr)
        this->local_addr = (char *)::malloc(INET6_ADDRSTRLEN); // TODO optimize
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
int http_frontend::to_connect_backend() {
    this->matched_app->accepted_num.fetch_add(1, std::memory_order_relaxed);
    app::backend *ab = nullptr;
    if (this->matched_app->cf->balance_policy == app::roundrobin)
        ab = this->matched_app->get_backend_by_smooth_wrr(); // no need to check for nullptr
    if (ab == nullptr)
        return -1;
    
    struct sockaddr_in taddr;
    inet_addr::parse_v4_addr(ab->host, &taddr);
    nbx_inet_addr naddr{(struct sockaddr*)&taddr, sizeof(taddr)};
    this->backend_conn = new backend(this->wrker, this, this->matched_app);
    if (this->wrker->conn->connect(this->backend_conn, naddr,
            this->matched_app->cf->connect_backend_timeout) == -1) {
        delete this->backend_conn;
        this->backend_conn = nullptr;
        log::warn("connect to backend:%s fail!", ab->host.c_str());
        return -1;
    }
    return 0;
}
// NOTE frontend & backend 不能在各自的执行栈中操作对方的资源,这样会导致资源管理混乱
// poller中有ready_events 队列, 有可能backend另一个事件已经wait到了
// 交由taskq统一释放, 这样不受wait list影响
void http_frontend::backend_connect_ok() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件

    if (this->state == conn_ok)
        this->wrker->push_task(task_in_worker(task_in_worker::backend_conn_ok, this));
}
void http_frontend::on_backend_connect_ok() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件
    int fd = this->get_fd();
    if (fd == -1)
        return ;
    socket::set_nodelay(fd);
    if (this->wrker->add_ev(this, fd, ev_handler::ev_read) != 0) {
        log::error("new http_frontend add to poller fail! %s", strerror(errno));
        this->on_close();
        return ;
    }
    this->matched_app->frontend_active_n.fetch_add(1, std::memory_order_relaxed);
    this->state = active_ok;
}
void http_frontend::backend_connect_fail() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件
    
    this->backend_conn = nullptr;
    this->wrker->push_task(task_in_worker(task_in_worker::backend_conn_fail, this));
}
void http_frontend::on_backend_connect_fail() {
    if (this->state == closed)
        return ;

    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void http_frontend::backend_close() {
    if (this->backend_conn == nullptr)
        return ; // 如果早就解除关系了, 就忽略它的事件
    
    this->backend_conn = nullptr;
    this->wrker->push_task(task_in_worker(task_in_worker::backend_close, this));
}
void http_frontend::on_backend_close() {
    if (this->state == closed)
        return ;

    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_all);
    this->on_close();
}
void http_frontend::on_close() { // maybe trigger EPOLLHUP | EPOLLERR
    if (this->matched_app != nullptr)
        if (this->state == active_ok)
            this->matched_app->frontend_active_n.fetch_sub(1, std::memory_order_relaxed);
    
    if (this->backend_conn != nullptr) {
        this->backend_conn->frontend_close();
        this->backend_conn = nullptr; // 解除关系
    }
    this->wrker->push_task(task_in_worker(task_in_worker::del_ev_handler, this));
    this->destroy();
    this->state = closed;
}
bool http_frontend::on_read() {
    if (unlikely(this->backend_conn == nullptr))
        return false;

    char *buf = nullptr;
    int ret = this->recv(buf);
    if (likely(ret > 0))
        return this->handle_request(buf, ret);
    if (ret == 0) // closed
        return false;
    return true; // ret < 0
}
bool http_frontend::handle_request(const char *rbuf, int rlen) {
    if (!this->matched_app->cf->with_x_real_ip
        && !this->matched_app->cf->with_x_forwarded_for) {
        this->backend_conn->send(rbuf, rlen);
        return true;
    }
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
    http_parser parser;
    parser.reset(rbuf, rlen);
    int ret = 0;
    int left_len = rlen;
    int request_line_len = 0;
    do {
        ret = parser.parse_request_line();
        if (ret > 0) {
            const char *sbf = http::err_msgs[ret];
            this->send(sbf, ::strlen(sbf));
            return false;
        } else if (ret == -1) { // partial
            if (parser.start - parser.req_start > MAX_URI_LEN) {
                const char *sbf = http::err_msgs[HTTP_ERR_400];
                this->send(sbf, ::strlen(sbf));
                return false;
            }
            if (this->partial_buf_len == 0) {
                left_len = rlen - (parser.req_start - rbuf);
                this->partial_buf = (char*)::malloc(left_len);
                ::memcpy(this->partial_buf, parser.req_start, left_len);
                this->partial_buf_len = left_len;
            }
            return true;
        }
        request_line_len = parser.start - parser.req_start;
        bool has_x_real_ip = false;
        const char *xff_start = nullptr;
        int xff_len = 0;
        do {
            ret = parser.parse_header_line();
            if (ret == 0) {
                char ch = *(parser.header_name_start);
                if (LOWER(ch) == 'x') {
                    if (this->matched_app->cf->with_x_real_ip
                        && !has_x_real_ip
                        && (parser.header_name_end - parser.header_name_start)
                            == (int)sizeof("X-Real-IP") - 1
                        && ::strncasecmp(parser.header_name_start, "X-Real-IP",
                            sizeof("X-Real-IP") - 1) == 0) {
                        has_x_real_ip = true;
                    } else if (this->matched_app->cf->with_x_forwarded_for
                        && (parser.header_name_end - parser.header_name_start)
                        == (int)sizeof("X-Forwarded-For") - 1
                        && ::strncasecmp(parser.header_name_start, "X-Forwarded-For",
                            sizeof("X-Forwarded-For") - 1) == 0) {
                        xff_start = parser.header_name_start;
                        xff_len = parser.value_end - parser.header_name_start;
                    }
                }
            } else if (ret > 0) {
                const char *sbf = http::err_msgs[ret];
                this->send(sbf, ::strlen(sbf));
                return false;
            } else if (ret == -1) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN) {
                    const char *sbf = http::err_msgs[HTTP_ERR_400];
                    this->send(sbf, ::strlen(sbf));
                    return false;
                }
                if (this->partial_buf_len == 0) {
                    left_len = rlen - (parser.req_start - rbuf);
                    this->partial_buf = (char*)::malloc(left_len);
                    ::memcpy(this->partial_buf, parser.req_start, left_len);
                    this->partial_buf_len = left_len;
                }
                return true;
            } else if (ret == -2) {
                this->a_complete_request(parser.req_start,
                    parser.start - parser.req_start, request_line_len,
                    has_x_real_ip, xff_start, xff_len);
                break;
            }
        } while (true);
        if (parser.start >= rbuf + rlen)
            break;
        parser.reset(parser.start, rlen - (parser.start - rbuf));
    } while (true);
    return true;
}
#define save_partial_buf() if (this->partial_buf_len == 0) { \
                               this->partial_buf = (char*)::malloc(len); \
                               ::memcpy(this->partial_buf, buf, len - buf_offset); \
                               this->partial_buf_len = len; \
                           }

bool http_frontend::handle_request2(const char *rbuf, int rlen) {
    if (!this->matched_app->cf->with_x_real_ip
        && !this->matched_app->cf->with_x_forwarded_for) {
        this->backend_conn->send(rbuf, rlen);
        return true;
    }
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
        const char *xff_start = nullptr;
        int xff_len = 0;
        if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
            if (buf[3] != ' ')
                return false; // invalid
            this->method = 1;
            buf_offset += 4;
        } else if (buf[0] == 'P' && buf[1] == 'O' && buf[3] == 'T') {
            if (buf[4] != ' ')
                return false; // invalid
            this->method = 2;
            buf_offset += 5;
        }
        if (this->method == 0)
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
            this->a_complete_request(buf, buf_offset+1+1, header_line_end, false, nullptr, 0);
            return true;
        }

        bool has_x_real_ip = false;
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
            if (LOWER(*start) == 'x') {
                if (this->matched_app->cf->with_x_real_ip
                    && !has_x_real_ip
                    && p - start + 1 >= (int)sizeof("X-Real-IP:") - 1
                    && ::strncasecmp(start, "X-Real-IP:", sizeof("X-Real-IP:") - 1) == 0) {
                    has_x_real_ip = true;
                } else if (this->matched_app->cf->with_x_forwarded_for
                    && p - start + 1 >= (int)sizeof("X-Forwarded-For:0.0.0.0") - 1
                    && ::strncasecmp(start, "X-Forwarded-For:", sizeof("X-Forwarded-For:") - 1) == 0) {
                    xff_start = start;
                    xff_len = p - 1 - start;
                }
            }
            buf_offset += p - start + 1;
            if (xff_start != nullptr)
                break ;
        }

        if (len - buf_offset < 2) { // no end of \r\n ?
            save_partial_buf();
            return true;
        }
        if (buf[buf_offset] == '\r' && buf[buf_offset+1] == '\n')
            this->a_complete_request(buf, buf_offset+1+1, header_line_end,
                has_x_real_ip, xff_start, xff_len);

        buf_offset += 2;
    } while (true);

    return true;
}
int http_frontend::a_complete_request(const char *buf, const int len,
    const int request_line_len,
    const bool has_x_real_ip,
    const char *xff_start,
    const int xff_len) {

    char hbuf[MAX_FULL_REQ_LEN];
    int copy_len = 0;

    ::memcpy(hbuf, buf, request_line_len);
    copy_len += request_line_len;
    if (has_x_real_ip == false && this->matched_app->cf->with_x_real_ip) {
        ::memcpy(hbuf + copy_len, "X-Real-IP: ", sizeof("X-Real-IP: ") - 1);
        copy_len += sizeof("X-Real-IP: ") - 1;
        ::memcpy(hbuf + copy_len, this->remote_addr, this->remote_addr_len);
        copy_len += this->remote_addr_len;
        ::memcpy(hbuf + copy_len, "\r\n", 2);
        copy_len += 2;
    }

    if (this->matched_app->cf->with_x_forwarded_for) {
        if (xff_start == nullptr) { // add
            ::memcpy(hbuf + copy_len, "X-Forwarded-For: ", sizeof("X-Forwarded-For: ") - 1);
            copy_len += sizeof("X-Forwarded-For: ") - 1;
            ::memcpy(hbuf + copy_len, this->remote_addr, this->remote_addr_len);
            copy_len += this->remote_addr_len;
        } else {
            ::memcpy(hbuf + copy_len, xff_start, xff_len);
            copy_len += xff_len;
            ::memcpy(hbuf + copy_len, ", ", 2);
            copy_len += 2;
            ::memcpy(hbuf + copy_len, this->local_addr, this->local_addr_len);
            copy_len += this->local_addr_len;
        }
        ::memcpy(hbuf + copy_len, "\r\n", 2);
        copy_len += 2;
    }

    bool copy_all = false;
    if (len - request_line_len <= ((int)sizeof(hbuf) - copy_len)) {
        ::memcpy(hbuf + copy_len, buf + request_line_len, len - request_line_len);
        copy_len += len - request_line_len;
        copy_all = true;
    }

    this->backend_conn->send(hbuf, copy_len);
    if (copy_all == false)
        this->backend_conn->send(buf + request_line_len, len - request_line_len);
    return true;
}
