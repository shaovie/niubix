#ifndef NBX_IO_HANDLE_H_
#define NBX_IO_HANDLE_H_

#include "ev_handler.h"
#include "ringq.h"

class async_send_buf {
public:
    async_send_buf() = default;
    async_send_buf(char *bf, const int l): len(l), buf(bf) { }
    async_send_buf(const async_send_buf &v): sendn(v.sendn), len(v.len), buf(v.buf) { }
    async_send_buf& operator=(const async_send_buf &v) {
        this->buf = v.buf;
        this->len = v.len;
        this->sendn = v.sendn;
        return *this;
    }

    int sendn = 0;
    int len   = 0;
    char *buf = nullptr;
};

class io_handle : public ev_handler {
public:
    io_handle() = default;

    int recv(char* &buff);
    int send(const char *buff, const int len);

    virtual bool on_write();

    void destroy();
private:
    bool async_send_polling = false;
    int async_send_buf_size = 0;
    ringq<async_send_buf> *async_send_buf_q = nullptr;
};

#endif // NBX_IO_HANDLE_H_
