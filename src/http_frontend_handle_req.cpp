#include "http_frontend.h"
#include "socket.h"
#include "app.h"
#include "log.h"
#include "defines.h"
#include "backend.h"
#include "http_parser.h"

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
    const char *req_start = nullptr;
    const char *req_end = nullptr;
    const char *xff_start = nullptr;
    const char *xff_end = nullptr;
    const char *xff_next_pos = nullptr;
};
bool http_frontend::handle_request(const char *rbuf, int rlen) {
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
        if (ret == http_parser::partial_req) { // partial
            if (parser.start - parser.req_start > MAX_URI_LEN)
                return this->response_err_and_close(HTTP_ERR_400);
            this->handle_partial_req(parser, rbuf, rlen);
            return true;
        } else if (ret != http_parser::parse_ok)
            return this->response_err_and_close(ret);
        if (parser.method == http_post || parser.method == http_put)
            return this->response_err_and_close(HTTP_ERR_405);
        
        parse_req_result prr;
        prr.req_start = parser.req_start;
        do {
            ret = parser.parse_header_line();
            if (ret == http_parser::parse_ok) {
                if (LOWER(*(parser.header_name_start)) != 'x')
                    continue ;
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
            } else if (ret == http_parser::partial_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                this->handle_partial_req(parser, rbuf, rlen);
                return true;
            } else if (ret == http_parser::eof) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                prr.req_end = parser.header_end;
                this->a_complete_get_req(prr);
                break;
            } else
                return this->response_err_and_close(ret);
        } while (true); // parse header line

        if (parser.start >= rbuf + rlen) // end
            break;
    } while (true);
    return true;
}
int http_frontend::a_complete_get_req(const parse_req_result &prr) {
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
    return true;
}
