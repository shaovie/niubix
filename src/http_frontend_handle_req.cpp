#include "http_frontend.h"
#include "socket.h"
#include "worker.h"
#include "app.h"
#include "log.h"
#include "defines.h"
#include "backend.h"
#include "http_parser.h"

#include <climits>
#include <cstring>

void http_frontend::handle_partial_req(const http_parser &parser,
    const char *rbuf, const int rlen) {
    if (this->partial_buf_len == 0) {
        int left_len = rlen - (parser.req_start - rbuf);
        this->partial_buf = (char*)::malloc(left_len);
        ::memcpy(this->partial_buf, parser.req_start, left_len);
        this->partial_buf_len = left_len;
    }
}
class parse_req_result {
public:
    parse_req_result() = default;
    bool x_real_ip_exist = false;
    uint64_t content_length = 0;
    int payload_len = 0;
    http_parser *parser = nullptr; 
    const char *xff_start = nullptr;
    const char *xff_end = nullptr;
    const char *payload = nullptr;
};
bool http_frontend::handle_data(const char *rbuf, int rlen) {
    this->recv_time = this->wrker->now_msec;
    if (this->content_length > 0) {
        int payload_len = std::min((uint64_t)rlen, this->content_length);
        this->forward_to_backend(rbuf, payload_len);
        this->content_length -= payload_len;
        rlen -= payload_len;
        if (rlen == 0)
            return true;
        rbuf += payload_len; // shift offset
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
    return this->handle_request(rbuf, rlen);
}
bool http_frontend::handle_request(const char *rbuf, int rlen) {
    http_parser parser(rbuf, rbuf + rlen);
    int ret = 0;
    do {
        ret = parser.parse_request_line();
        if (unlikely(ret == http_parser::partial_req)) { // partial
            if (parser.start - parser.req_start > MAX_URI_LEN)
                return this->response_err_and_close(HTTP_ERR_400);
            this->handle_partial_req(parser, rbuf, rlen);
            return true;
        } else if (ret != http_parser::parse_ok) {
            ((char *)rbuf)[rlen] = '\0';
            log::warn("invalid req [%s] code=%d", parser.req_start, ret);
            return this->response_err_and_close(ret);
        }
        
        parse_req_result prr;
        prr.parser = &parser;
        do {
            ret = parser.parse_header_line();
            if (ret == http_parser::parse_ok) {
                ret = this->filter_headers(parser, prr);
                if (ret != 0)
                    return this->response_err_and_close(ret);
            } else if (ret == http_parser::partial_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                this->handle_partial_req(parser, parser.req_start,
                    parser.start - parser.req_start);
                return true;
            } else if (ret == http_parser::end_of_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                if (this->matched_app == nullptr) { // must parsed host
                    // assert host != nullptr
                    this->save_received_data_before_match_app(rbuf, rlen);
                    // 暂停接收消息(对端关闭的事件也捕捉不到了)
                    this->wrker->remove_ev(this->get_fd(), ev_handler::ev_read);
                    this->to_match_app_by_host();
                    return true;
                }
                if (this->content_length > 0) { 
                    prr.payload = parser.start;
                    prr.payload_len = std::min(this->content_length,
                        (uint64_t)(rlen - (prr.payload - rbuf)));
                    parser.start += prr.payload_len;
                }
                this->a_complete_req(prr);
                break;
            } else
                return this->response_err_and_close(ret);
        } while (true); // parse header line

        if (parser.start >= rbuf + rlen) // end
            break;
    } while (true);
    return true;
}
int http_frontend::filter_headers(const http_parser &parser, parse_req_result &prr) {
    if (this->host == nullptr && LOWER(*(parser.header_name_start)) == 'h') {
        // 每个链接只解析一次host, 应该不会有的request Host会变化吧?
        if (parser.header_name_end - parser.header_name_start == (int)sizeof("Host") - 1
            && ::strncasecmp(parser.header_name_start, "Host", sizeof("Host") - 1) == 0
            && parser.value_start != nullptr) {
            int host_len = parser.value_end - parser.value_start;
            if (host_len > 0) {
                auto sep = (const char *)::memchr(parser.value_start, ':', host_len);
                if (sep != nullptr)
                    host_len = sep - parser.value_start;
                if (host_len > 0) {
                    this->host = (char *)::malloc(host_len + 1);
                    ::memcpy(this->host, parser.value_start, host_len);
                    this->host[host_len] = '\0';
                }
            }
        }
        if (this->host == nullptr) // client must include Host header in HTTP 1.1.
            return HTTP_ERR_400; // no host
        return 0;
    }
    if (this->matched_app != nullptr && LOWER(*(parser.header_name_start)) == 'x') {
        if (this->matched_app->cf->with_x_real_ip
            && !prr.x_real_ip_exist
            && (parser.header_name_end - parser.header_name_start)
                == (int)sizeof("X-Real-IP") - 1
            && ::strncasecmp(parser.header_name_start, "X-Real-IP",
                sizeof("X-Real-IP") - 1) == 0) {
            prr.x_real_ip_exist = true;
        } else if (this->matched_app->cf->with_x_forwarded_for
            && prr.xff_start == nullptr
            && (parser.header_name_end - parser.header_name_start)
                == (int)sizeof("X-Forwarded-For") - 1
            && ::strncasecmp(parser.header_name_start, "X-Forwarded-For",
                sizeof("X-Forwarded-For") - 1) == 0) {
            prr.xff_start = parser.header_name_start;
            prr.xff_end = parser.value_end;
        }
        return 0;
    }
    if (parser.method == http_post || parser.method == http_put) {
        if ((parser.header_name_end - parser.header_name_start)
                == (int)sizeof("Content-Length") - 1
            && ::strncasecmp(parser.header_name_start, "Content-Length",
                sizeof("Content-Length") - 1) == 0) {
            if (parser.value_start == nullptr)
                return HTTP_ERR_411;
            if (parser.value_end - parser.value_start > (int)sizeof("9223372036854775807") - 1)
                return this->response_err_and_close(HTTP_ERR_413);
            if (*parser.value_start == '-')
                return this->response_err_and_close(HTTP_ERR_400);
            prr.content_length = ::strtoll(parser.value_start, nullptr, 10);
            if (prr.content_length == LLONG_MAX || errno == ERANGE)
                return this->response_err_and_close(HTTP_ERR_413);
            this->content_length = prr.content_length;
            return 0;
        }
    }
    return 0;
}
int http_frontend::a_complete_req(parse_req_result &prr) {
    this->a_complete_req_time = this->wrker->now_msec;

    if (this->matched_app == nullptr)
        return 0;

    const int extra_len = sizeof("X-Real-IP: ") - 1 + INET6_ADDRSTRLEN + 2/*CRLF*/
        + sizeof("X-Forwarded-For: ") - 1 + INET6_ADDRSTRLEN + 2;
    char hbuf[MAX_FULL_REQ_LEN + extra_len];
    int copy_len = 0;

    if (this->matched_app->cf->with_x_forwarded_for) {
        if (prr.xff_start != nullptr) {
            ::memcpy(hbuf + copy_len, prr.parser->req_start,
                prr.xff_end - prr.parser->req_start);
            copy_len += prr.xff_end - prr.parser->req_start;
            ::memcpy(hbuf + copy_len, ", ", 2);
            copy_len += 2;
            ::memcpy(hbuf + copy_len, this->local_addr, this->local_addr_len);
            copy_len += this->local_addr_len;
            ::memcpy(hbuf + copy_len, "\r\n", 2);
            copy_len += 2;
        } else {
            ::memcpy(hbuf, prr.parser->req_start,
                prr.parser->req_end - prr.parser->req_start);
            copy_len += prr.parser->req_end - prr.parser->req_start;

            ::memcpy(hbuf + copy_len, "X-Forwarded-For: ", sizeof("X-Forwarded-For: ") - 1);
            copy_len += sizeof("X-Forwarded-For: ") - 1;
            ::memcpy(hbuf + copy_len, this->remote_addr, this->remote_addr_len);
            copy_len += this->remote_addr_len;
            ::memcpy(hbuf + copy_len, "\r\n", 2);
            copy_len += 2;
        }
    }

    if (this->matched_app->cf->with_x_real_ip && prr.x_real_ip_exist == false)  {
        if (copy_len == 0) {
            ::memcpy(hbuf, prr.parser->req_start,
                prr.parser->req_end - prr.parser->req_start);
            copy_len += prr.parser->req_end - prr.parser->req_start;
        }
        ::memcpy(hbuf + copy_len, "X-Real-IP: ", sizeof("X-Real-IP: ") - 1);
        copy_len += sizeof("X-Real-IP: ") - 1;
        ::memcpy(hbuf + copy_len, this->remote_addr, this->remote_addr_len);
        copy_len += this->remote_addr_len;
        ::memcpy(hbuf + copy_len, "\r\n", 2);
        copy_len += 2;
    }
    if (copy_len == 0) { // above nop
        this->forward_to_backend(prr.parser->req_start,
            prr.parser->req_end_crlf - prr.parser->req_start);
        return true;
    }
    ::memcpy(hbuf + copy_len, "\r\n", 2);
    copy_len += 2;

    // 算一下hbuf栈空间还有多少剩余, 尽量一次send完
    int hbuf_left_size = sizeof(hbuf) - copy_len;
    if (hbuf_left_size >= prr.payload_len) {
        ::memcpy(hbuf + copy_len, prr.payload, prr.payload_len);
        copy_len += prr.payload_len;
        this->content_length -= prr.payload_len;
        prr.payload_len = 0;
    }

    this->forward_to_backend(hbuf, copy_len);
    if (prr.payload_len > 0) { // payload
        this->forward_to_backend(prr.payload, prr.payload_len);
        this->content_length -= prr.payload_len;
    }
    return true;
}
