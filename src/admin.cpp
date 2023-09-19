#include "admin.h"
#include "log.h"
#include "app.h"
#include "worker.h"
#include "socket.h"
#include "http_parser.h"
#include "global.h"
#include "conf.h"

#include "nlohmann/json.hpp"

#include <signal.h>
#include <sys/types.h>
#include <map>

void parse_query_params(std::map<std::string, std::string> &params,
    const char *query, const int len) {

    char *tok_p = NULL;
    char *token = NULL;
    char bf[MAX_URI_LEN] = {0};
    ::strncpy(bf, query, len);
    bf[len] = '\0';
    for (token = ::strtok_r(bf, "&", &tok_p); 
        token != NULL;
        token = ::strtok_r(NULL, "&", &tok_p)) {
        char *p = ::strchr(token, '=');
        if (p == nullptr)
            params[token] = "";
        else {
            *p = '\0';
            params[token] = std::string(p + 1);
        }
    }
}

admin::~admin() {
    if (this->remote_addr != nullptr)
        ::free(this->remote_addr);
}
void admin::set_remote_addr(const struct sockaddr *addr, const socklen_t) {
    if (this->remote_addr == nullptr)
        this->remote_addr = (char *)::malloc(INET6_ADDRSTRLEN); // TODO optimize
    this->remote_addr[INET6_ADDRSTRLEN-1] = '\0';
    if (socket::addr_to_string(addr, this->remote_addr, INET6_ADDRSTRLEN) == 0)
        this->remote_addr_len = ::strlen(this->remote_addr);
}
void admin::response_error(const int code, const char *msg) {
    nlohmann::json resp;
    resp["code"] = code;
    resp["msg"] = msg;
    this->response_json(resp.dump());
}
void admin::response_json(const std::string &data) {
    int resp_len = ::snprintf(this->wrker->wio_buf, this->wrker->wio_buf_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-length: %lu\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "%s",  data.length(), data.c_str());
    if (resp_len >= this->wrker->wio_buf_size)
        resp_len = this->wrker->wio_buf_size - 1;
    this->send(this->wrker->wio_buf, resp_len);
}
bool admin::on_open() {
    if (!g::cf->admin_ip_white_set.empty()) {
        if (g::cf->admin_ip_white_set.count(this->remote_addr) == 0) {
            this->response_error(http::err_codes[HTTP_ERR_403], "Forbidden");
            return false;
        }
    }
    int fd = this->get_fd();
    socket::set_nodelay(fd);
    if (this->wrker->add_ev(this, fd, ev_handler::ev_read) != 0) {
        log::error("new admin conn add to poller fail! %s", strerror(errno));
        return false;
    }
    return true;
}
void admin::on_close() { // maybe trigger EPOLLHUP | EPOLLERR
    this->destroy();
    delete this;
}
bool admin::on_read() {
    char *buf = nullptr;
    int ret = this->recv(buf);
    if (likely(ret > 0))
        return this->handle_request(buf, ret);
    if (ret == 0) // closed
        return false;
    return true; // ret < 0
}
bool admin::handle_request(const char *rbuf, int rlen) {
    http_parser parser(rbuf, rbuf + rlen);
    int ret = 0;
    do {
        ret = parser.parse_request_line();
        if (unlikely(ret == http_parser::partial_req)) { // partial
            this->response_error(http::err_codes[HTTP_ERR_400], "Bad request");
            return false;
        } else if (ret != http_parser::parse_ok) {
            log::warn("invalid admin req [%s] code=%d", parser.req_start, ret);
            this->response_error(http::err_codes[ret], "Bad request");
            return false;
        }
        
        do {
            ret = parser.parse_header_line();
            if (ret == http_parser::parse_ok) {
                continue ;
            } else if (ret == http_parser::partial_req) {
                this->response_error(http::err_codes[HTTP_ERR_400], "Bad request");
                return false;
            } else if (ret == http_parser::end_of_req) {
                if (parser.start - parser.req_start > MAX_FULL_REQ_LEN) {
                    this->response_error(http::err_codes[HTTP_ERR_400], "Bad request");
                    return false;
                }
                ret = this->a_complete_req(parser);
                if (ret != HTTP_ERR_200) {
                    this->response_error(http::err_codes[ret], "Bad request");
                    return false;
                }
                break;
            } else {
                this->response_error(http::err_codes[ret], "Bad request");
                return false;
            }
        } while (true); // parse header line

        if (parser.start >= rbuf + rlen) // end
            break;
    } while (true);
    return true;
}
int admin::a_complete_req(const http_parser &parser) {
    if (parser.uri_end - parser.uri_start > MAX_URI_LEN-1)
        return HTTP_ERR_400;
    char uri_buf[MAX_URI_LEN] = {0};
    int uri_len = parser.uri_end - parser.uri_start;
    ::strncpy(uri_buf, parser.uri_start, uri_len);
    uri_buf[uri_len] = '\0';
    http::url_decode(uri_buf, uri_len);

    const char *query_start = nullptr;
    const char *query_end = nullptr;
    const char *path_end = nullptr;
    http_parser parser2;
    parser2.uri_start = uri_buf;
    parser2.uri_end = uri_buf + uri_len;
    int ret = parser2.parse_uri(path_end, query_start, query_end);
    if (ret != 0)
        return ret;
    
    if (::strncmp(uri_buf, "/set_backend_down", path_end - uri_buf) == 0)
        this->set_backend_down(query_start, query_end);
    else if (::strncmp(uri_buf, "/shutdown", path_end - uri_buf) == 0)
        this->shutdown();
    else if (::strncmp(uri_buf, "/reload", path_end - uri_buf) == 0)
        this->reload();
    else
        return HTTP_ERR_404;

    return HTTP_ERR_200;
}
void admin::set_backend_down(const char *query_start, const char *query_end) {
    std::map<std::string, std::string> params;
    parse_query_params(params, query_start, query_end - query_start);

    const auto &name = params["name"];
    const auto &host = params["host"];
    const auto &down = params["down"];
    if (name.empty() || host.empty() || (down != "true" && down != "false")) {
        this->response_error(http::err_codes[HTTP_ERR_400], "param invalid");
        return ;
    }
    bool set_ok = false;
    for (auto &ap : app::alls) {
        if (ap->cf->name == name) {
            set_ok = ap->set_backend_down(host, down == "true");
            break;
        }
    }
    nlohmann::json resp;
    resp["code"] = 0;
    resp["msg"] = "ok";
    resp["name"] = name;
    resp["host"] = host;
    resp["down"] = down;
    if (set_ok != true) {
        resp["code"] = 404;
        resp["msg"] = "not found";
    }
    this->response_json(resp.dump());
}
extern void let_worker_shutdown(int);
void admin::shutdown() {
    let_worker_shutdown(0);

    nlohmann::json resp;
    resp["code"] = 0;
    resp["msg"] = "shutdowning";
    this->response_json(resp.dump());
}
void admin::reload() {
    ::kill(g::master_pid, SIGHUP);

    nlohmann::json resp;
    resp["code"] = 0;
    resp["msg"] = "reloading";
    this->response_json(resp.dump());
}
