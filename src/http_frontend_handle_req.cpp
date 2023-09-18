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
    const char *req_start = nullptr;
    const char *req_end = nullptr;
    const char *xff_start = nullptr;
    const char *xff_end = nullptr;
    const char *xff_next_pos = nullptr;
    const char *payload = nullptr;
};
bool http_frontend::handle_request(const char *rbuf, int rlen) {
    this->recv_time = this->wrker->now_msec;
    if (this->content_length > 0) {
        int payload_len = std::min((uint64_t)rlen, this->content_length);
        this->backend_conn->send(rbuf, payload_len);
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
            log::warn("invalid req [%s] code=%d", parser.req_start, ret);
            return this->response_err_and_close(ret);
        }
        
        parse_req_result prr;
        prr.req_start = parser.req_start;
        do {
            ret = parser.parse_header_line();
            if (ret == http_parser::parse_ok) {
                if (this->filter_headers(parser, prr) == false)
                    return false;
            } else if (ret == http_parser::partial_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                this->handle_partial_req(parser, parser.req_start,
                    parser.start - parser.req_start);
                return true;
            } else if (ret == http_parser::end_of_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                prr.req_end = parser.header_end;
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
bool http_frontend::filter_headers(const http_parser &parser, parse_req_result &prr) {
    if (LOWER(*(parser.header_name_start)) == 'x') {
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
            prr.xff_next_pos = parser.start;
        }
    }
    if (parser.method == http_post || parser.method == http_put) {
        if ((parser.header_name_end - parser.header_name_start)
                == (int)sizeof("Content-Length") - 1
            && ::strncasecmp(parser.header_name_start, "Content-Length",
                sizeof("Content-Length") - 1) == 0) {
            if (parser.value_end - parser.value_start > (int)sizeof("9223372036854775807") - 1)
                return this->response_err_and_close(HTTP_ERR_413);
            if (*parser.value_start == '-')
                return this->response_err_and_close(HTTP_ERR_400);
            prr.content_length = ::strtoll(parser.value_start, nullptr, 10);
            if (prr.content_length == LLONG_MAX || errno == ERANGE)
                return this->response_err_and_close(HTTP_ERR_413);
            this->content_length = prr.content_length;
        }
    }
    return true;
}
int http_frontend::a_complete_req(const parse_req_result &prr) {
    this->a_complete_req_time = this->wrker->now_msec;
    const int extra_len = sizeof("X-Real-IP: ") - 1 + INET6_ADDRSTRLEN + 2/*CRLF*/
        + sizeof("X-Forwarded-For: ") - 1 + INET6_ADDRSTRLEN + 2;
    char hbuf[MAX_FULL_REQ_LEN + extra_len];
    int copy_len = 0;

    if (this->matched_app->cf->with_x_forwarded_for) {
        if (prr.xff_start != nullptr) {
            ::memcpy(hbuf + copy_len, prr.req_start, prr.xff_end - prr.req_start);
            copy_len += prr.xff_end - prr.req_start;
            ::memcpy(hbuf + copy_len, ", ", 2);
            copy_len += 2;
            ::memcpy(hbuf + copy_len, this->local_addr, this->local_addr_len);
            copy_len += this->local_addr_len;
            ::memcpy(hbuf + copy_len, "\r\n", 2);
            copy_len += 2;
        } else {
            ::memcpy(hbuf, prr.req_start, prr.req_end - prr.req_start);
            copy_len += prr.req_end - prr.req_start;

            ::memcpy(hbuf + copy_len, "X-Forwarded-For: ", sizeof("X-Forwarded-For: ") - 1);
            copy_len += sizeof("X-Forwarded-For: ") - 1;
            ::memcpy(hbuf + copy_len, this->remote_addr, this->remote_addr_len);
            copy_len += this->remote_addr_len;
            ::memcpy(hbuf + copy_len, "\r\n", 2);
            copy_len += 2;
        }
    }

    if (this->matched_app->cf->with_x_real_ip && prr.x_real_ip_exist == false)  {
        ::memcpy(hbuf + copy_len, "X-Real-IP: ", sizeof("X-Real-IP: ") - 1);
        copy_len += sizeof("X-Real-IP: ") - 1;
        ::memcpy(hbuf + copy_len, this->remote_addr, this->remote_addr_len);
        copy_len += this->remote_addr_len;
        ::memcpy(hbuf + copy_len, "\r\n", 2);
        copy_len += 2;
    }
    ::memcpy(hbuf + copy_len, "\r\n", 2);
    copy_len += 2;

    this->backend_conn->send(hbuf, copy_len);
    if (prr.payload_len > 0) {
        this->backend_conn->send(prr.payload, prr.payload_len);
        this->content_length -= prr.payload_len;
    }
    return true;
}
