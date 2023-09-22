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

void http_frontend::handle_partial_req(const char *req_start, const int left_len) {
    if (this->partial_buf_len == 0) {
        if (left_len > 0) {
            this->partial_buf = (char*)::malloc(left_len);
            ::memcpy(this->partial_buf, req_start, left_len);
            this->partial_buf_len = left_len;
        }
    } else
        log::error("handle partial error. #001");
}
class parse_req_result {
public:
    parse_req_result() = default;

    bool x_real_ip_exist = false;
    int payload_len = 0;
    uint64_t content_length = 0;
    http_parser *parser = nullptr; 
    const char *xff_start = nullptr;
    const char *xff_end = nullptr;
    const char *payload = nullptr;
};
bool http_frontend::handle_data(const char *rbuf, int rlen) {
    this->recv_time = this->wrker->now_msec;
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

    if (this->content_length > 0) {
        int payload_len = std::min((uint64_t)rlen, this->content_length);
        this->forward_to_backend(rbuf, payload_len);
        this->content_length -= payload_len;
        rlen -= payload_len;
        if (rlen == 0)
            return true;
        rbuf += payload_len; // shift offset
    } else if (this->transfer_chunked == true) {
        int payload_len = std::min((uint64_t)rlen, this->chunk_size);
        this->forward_to_backend(rbuf, payload_len);
        this->chunk_size -= payload_len;
        rlen -= payload_len;
        if (rlen == 0)
            return true;
        rbuf += payload_len; // shift offset
        return this->handle_chunk(rbuf, rlen);
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

            this->handle_partial_req(parser.req_start, parser.start - parser.req_start);
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
            if (likely(ret == http_parser::parse_ok)) {
                ret = this->filter_headers(parser, prr);
                if (ret != 0)
                    return this->response_err_and_close(ret);
            } else if (ret == http_parser::partial_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);

                this->handle_partial_req(parser.req_start, parser.start - parser.req_start);
                return true;
            } else if (ret == http_parser::end_of_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN)
                    return this->response_err_and_close(HTTP_ERR_400);

                if (this->matched_app == nullptr) { // must parsed host
                    // assert host != nullptr
                    this->save_received_data_before_match_app(rbuf, rlen);

                    // 会暂停接收消息(对端关闭的事件也捕捉不到了)
                    return this->to_match_app_by_host();
                }
                // stream content.
                if (this->content_length > 0) { 
                    prr.payload = parser.start;
                    prr.payload_len = std::min(this->content_length,
                        (uint64_t)(rlen - (prr.payload - rbuf)));
                    parser.start += prr.payload_len;
                } else if (this->transfer_chunked == true) { // chunked content.
                    ret = this->handle_first_chunk(parser, prr); 
                    this->transfer_chunked = false;
                    this->chunk_size = 0;

                    if (unlikely(ret < 0)) // invalid req
                        return this->response_err_and_close(-ret);

                    if (ret == http_parser::chunked_ret::partial_chunk) {
                        this->handle_partial_req(parser.req_start, parser.start-parser.req_start);
                        this->transfer_chunked = true;
                        return true; // continue recv
                    }

                    if (ret == http_parser::chunked_ret::get_chunk_data)
                        this->transfer_chunked = true;
                    else if (ret != http_parser::chunked_ret::all_chunk_end)
                        return this->response_err_and_close(HTTP_ERR_400); // unknown error
                } // end of `if transfer_chunked == true'

                this->a_complete_req(prr);
                break; // #brk01
            } else // end of `if (ret == http_parser::end_of_req)'
                return this->response_err_and_close(ret);
        } while (true); // parse header line

        if (parser.start >= rbuf + rlen) // end
            break;
    } while (true);
    return true;
}
int http_frontend::handle_chunk(const char *rbuf, int rlen) {
    http_parser parser(rbuf, rbuf + rlen);
    parse_req_result prr;
    prr.parser = &parser;

    // TODO 研究一个更优雅的流程
    return 0;
}
int http_frontend::handle_first_chunk(http_parser &parser, parse_req_result &prr) {
    prr.payload = parser.start;
    http_parser::chunked_ret cr;
    int ret = 0;
    do {
        ret = parser.parse_chunked(&cr);
        if (ret != 0)
            return -ret;
        if (cr.result == http_parser::chunked_ret::partial_chunk)
            return http_parser::chunked_ret::partial_chunk;
        
        if (cr.result == http_parser::chunked_ret::get_chunk_data) {
            prr.payload = cr.data_start;
            if ((uint64_t)(parser.end - cr.data_start) >= cr.size) { // 超过一个完整的chunk
                cr.size = 0;
                parser.start = cr.data_start + cr.size; // skip chunk data
                continue ;
            }
            this->chunk_size = cr.size - (parser.start - cr.data_start); // save
            prr.payload_len = parser.start - prr.payload;
            return http_parser::chunked_ret::get_chunk_data;
        }

        // 解析完整个chunked-body, 可以去直接转发了
        if (cr.result == http_parser::chunked_ret::all_chunk_end) {
            prr.payload_len = parser.start - prr.payload;
            return http_parser::chunked_ret::all_chunk_end;
        }
        break;
    } while (true);

    log::error("handle first chunk error. #001");
    return -HTTP_ERR_400; // unknown error
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
                return HTTP_ERR_413;
            if (*parser.value_start == '-')
                return HTTP_ERR_400;

            prr.content_length = ::strtoll(parser.value_start, nullptr, 10);
            if (prr.content_length == LLONG_MAX || errno == ERANGE)
                return HTTP_ERR_413;
            this->content_length = prr.content_length;
            return 0;
        } else if ((parser.header_name_end - parser.header_name_start)
                == (int)sizeof("Transfer-Encoding") - 1
            && ::strncasecmp(parser.header_name_start, "Transfer-Encoding",
                sizeof("Transfer-Encoding") - 1) == 0) {

            if (parser.value_start == nullptr)
                return HTTP_ERR_400;
            if (::strncasecmp(parser.value_start, "chunked", sizeof("chunked") - 1) != 0)
                return HTTP_ERR_501;

            return HTTP_ERR_501; // TODO 还未完全实现
            this->transfer_chunked = true;
            return 0;
        }
    }
    // Transfer-Encoding: chunked
    return 0;
}
void http_frontend::a_complete_req(parse_req_result &prr) {
    this->a_complete_req_time = this->wrker->now_msec;

    if (this->matched_app == nullptr)
        return ;

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
    // forward http request header
    if (copy_len == 0) { // no extra info
        this->forward_to_backend(prr.parser->req_start,
            prr.parser->req_end_with_crlf - prr.parser->req_start);
        // payload sent separately. jump to #xc
    } else {
        ::memcpy(hbuf + copy_len, "\r\n", 2); // empty line
        copy_len += 2;

        // 算一下hbuf栈空间还有多少剩余, 尽量一次send完
        if (prr.payload_len > 0) {
            int hbuf_left_size = sizeof(hbuf) - copy_len;
            if (hbuf_left_size >= prr.payload_len) {
                ::memcpy(hbuf + copy_len, prr.payload, prr.payload_len);
                copy_len += prr.payload_len;
                this->content_length -= prr.payload_len;
                prr.payload_len = 0; // look #x1
            }
        }

        this->forward_to_backend(hbuf, copy_len);
    }
    // forward payload #xc
    if (prr.payload_len > 0) { // payload #x1
        this->forward_to_backend(prr.payload, prr.payload_len);
        this->content_length -= prr.payload_len;
    }
}
