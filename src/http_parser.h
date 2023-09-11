#ifndef NBX_HTTP_CONN_H_
#define NBX_HTTP_CONN_H_

class http_parser {
public:
    enum {
        ret_done = 0;
    };

    http_parser() = default;
    ~http_parser() {
        if (this->partial_buf != nullptr)
            ::free(this->partial_buf);
    }
    int parse(const char *buf, const int len);

    char *partial_buf = nullptr;
};
#endif // NBX_HTTP_CONN_H_
