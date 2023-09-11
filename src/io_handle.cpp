#include "io_handle.h"
#include "worker.h"
#include "defines.h"

#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>

void *io_handle::poll_cache_get(const int id) {
    return this->wrker->poll_cache_get(id);
}
int io_handle::recv(char* &buff) {
    if (unlikely(this->fd == -1))
        return -1;
    
    int ret = 0;
    do {
        ret = ::recv(this->fd, this->wrker->rio_buf, this->wrker->rio_buf_size, 0);
        if (ret > 0) {
            buff = this->wrker->rio_buf;
            break;
        }
    } while (ret == -1 && errno == EINTR);
    return ret;
}
int io_handle::send(const char *buff, const int len) {
    if (unlikely(this->fd == -1))
        return -1;
    
    // sync send in poller thread
    if (!this->async_send_buf_q.empty()) {
        char *bf = new char[len];
        ::memcpy(bf, buff, len);
        this->async_send_buf_q.push_back(async_send_buf(bf, len));
        this->async_send_buf_size += len;
        return len;
    }

    int ret = 0;
    do {
        ret = ::send(this->fd, buff, len, 0);
    } while (ret == -1 && errno == EINTR);

    if (ret < len) { // error or partial
        if (ret < 0)
            ret = 0;
        auto left = len - ret;
        char *bf = new char[left];
        ::memcpy(bf, buff + ret, left);
        this->async_send_buf_q.push_back(async_send_buf(bf, left));
        this->async_send_buf_size += left;
        if (this->async_send_polling == false) {
            this->wrker->append_ev(this->fd, ev_handler::ev_write);
            this->async_send_polling = true;
        }
        ret = len;
    }
    return ret;
}
bool io_handle::on_write() {
    if (this->fd == -1)
        return false; // goto on_close
                      
    int n = this->async_send_buf_q.length();
    for (auto i = 0; i < n; ++i) {
        async_send_buf &asb = this->async_send_buf_q.front();
        auto ret = ::send(this->fd, asb.buf + asb.sendn, asb.len - asb.sendn, 0);
        if (ret > 0) {
            this->async_send_buf_size -= ret;
            if (ret == (asb.len - asb.sendn)) {
                this->async_send_buf_q.pop_front();
                delete[] asb.buf;
                continue;
            }
            asb.sendn += ret;
        }
        break;
    }
    if (this->async_send_buf_q.empty() && this->async_send_polling == true) {
        this->wrker->remove_ev(this->fd, ev_handler::ev_write);
        this->async_send_polling = false;
    }
    return true;
}
void io_handle::sync_ordered_send(async_send_buf &asb) {
    if (this->fd == -1) {
        delete[] asb.buf;
        return;
    }
    this->async_send_buf_size += (asb.len - asb.sendn);
    if (!this->async_send_buf_q.empty()) {
        this->async_send_buf_q.push_back(asb);
        return ;
    }

    int ret = 0;
    do {
        ret = ::send(this->fd, asb.buf + asb.sendn, asb.len - asb.sendn, 0);
    } while (ret == -1 && errno == EINTR);

    if (ret > 0) { // partial
        this->async_send_buf_size -= ret;
        if (ret == (asb.len - asb.sendn)) {
            delete[] asb.buf;
            return ;
        }
        asb.sendn += ret;
    }
    this->async_send_buf_q.push_back(asb);
    if (this->async_send_polling == false) {
        this->wrker->append_ev(this->fd, ev_handler::ev_write);
        this->async_send_polling = true;
    }
}
void io_handle::destroy() {
    ev_handler::destroy();

    int n = this->async_send_buf_q.length();
    for (auto i = 0; i < n; ++i) {
        async_send_buf &asb = this->async_send_buf_q.front();
        delete[] asb.buf;
        this->async_send_buf_q.pop_front();
    }
    this->async_send_buf_size = 0;
}
