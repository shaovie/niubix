#ifndef NBX_CONNECTOR_H_
#define NBX_CONNECTOR_H_

#include "ev_handler.h"

// Forward declarations
class worker;
class nbx_inet_addr;

class connector final : public ev_handler
{
    friend class in_progress_connect;
public:
    connector() = delete;
    connector(worker *w) { this->set_worker(w); }

    // timeout is milliseconds e.g. 200ms;
    int connect(ev_handler *eh, const nbx_inet_addr &addr,
        const int timeout, const size_t rcvbuf_size = 0);
protected:
    int nblock_connect(ev_handler *eh, const int fd, const int timeout);
};

#endif // NBX_CONNECTOR_H_
