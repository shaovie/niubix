#include "http_frontend.h"
#include "app.h"
#include "log.h"
#include "defines.h"
#include "backend.h"
#include "http_parser.h"

#include <cstring>

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
    int left_len = rlen;
    int request_line_len = 0;
    do {
        ret = parser.parse_request_line();
        if (ret == http_parser::partial_req) { // partial
            if (parser.start - parser.req_start > MAX_URI_LEN)
                return this->response_err_and_close(HTTP_ERR_400);
            
            if (this->partial_buf_len == 0) {
                left_len = rlen - (parser.req_start - rbuf);
                this->partial_buf = (char*)::malloc(left_len);
                ::memcpy(this->partial_buf, parser.req_start, left_len);
                this->partial_buf_len = left_len;
            }
            return true;
        }
        if (parser.method == http_post || parser.method == http_put)
            return this->response_err_and_close(HTTP_ERR_405);
        
        request_line_len = parser.start - parser.req_start; // include CRLF
        bool has_x_real_ip = false;
        const char *xff_start = nullptr;
        int xff_len = 0;
        do {
            ret = parser.parse_header_line();
            if (ret == http_parser::parse_ok) {
                if (LOWER(*(parser.header_name_start)) != 'x')
                    continue ;
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
            } else if (ret == http_parser::partial_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);
                if (this->partial_buf_len == 0) {
                    left_len = rlen - (parser.req_start - rbuf);
                    this->partial_buf = (char*)::malloc(left_len);
                    ::memcpy(this->partial_buf, parser.req_start, left_len);
                    this->partial_buf_len = left_len;
                }
                return true;
            } else if (ret == http_parser::eof) {
                this->a_complete_request(parser.req_start,
                    parser.start - parser.req_start, request_line_len,
                    has_x_real_ip, xff_start, xff_len);
                break;
            } else
                return this->response_err_and_close(ret);
        } while (true);

        if (parser.start >= rbuf + rlen) // end
            break;
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
