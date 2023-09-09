#include "poll_desc.h"
#include "ev_handler.h"
#include "defines.h"
#include "evpoll.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

int evpoll::open(const int max_fds) {
    this->efd = ::epoll_create1(EPOLL_CLOEXEC);
    if (this->efd == -1) {
        log::error("epoll_create1 fail! %s", strerror(errno));
        return -1;
    }
    this->poll_descs = new poll_desc_map(max_fds);
    return 0;
}
int evpoll::add(ev_handler *eh, const int fd, const uint32_t ev) {
    if (unlikely(fd < 0))
        return -1;
    auto pd = this->poll_descs->alloc(fd);
    if (unlikely(pd == nullptr))
        return -1;
    
    pd->fd = fd;
    pd->eh = eh;

    struct epoll_event epev;
    epev.events = ev;
    epev.data.ptr = pd;

    if (::epoll_ctl(this->efd, EPOLL_CTL_ADD, fd, &epev) == 0) {
        this->active_num.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    this->poll_descs->del(fd);
    return -1;
}
int evpoll::append(const int fd, const uint32_t ev) {
    if (unlikely(fd < 0))
        return -1;
    auto pd = this->poll_descs->load(fd);
    if (unlikely(pd == nullptr))
        return -1;

    // Always save first, recover if failed  (this method is for multi-threading scenarios)."
    pd->events |= ev;
    
    struct epoll_event epev;
    epev.events = pd->events;
    epev.data.ptr = pd;
    if (::epoll_ctl(this->efd, EPOLL_CTL_MOD, fd, &epev) == 0)
        return 0;
    pd->events &= ~ev; // recover
    return -1;
}
int evpoll::remove(const int fd, const uint32_t ev) {
    if (unlikely(fd < 0))
        return -1;
    if (ev == ev_handler::ev_all) {
        this->poll_descs->del(fd);
        this->active_num.fetch_sub(1, std::memory_order_relaxed);
        return ::epoll_ctl(this->efd, EPOLL_CTL_DEL, fd, nullptr);
    }
    auto pd = this->poll_descs->load(fd);
    if (unlikely(pd == nullptr))
        return -1;
    if ((pd->events & (~ev)) == 0) {
        this->poll_descs->del(fd);
        this->active_num.fetch_sub(1, std::memory_order_relaxed);
        return ::epoll_ctl(this->efd, EPOLL_CTL_DEL, fd, nullptr);
    }

    // Always save first, recover if failed  (this method is for multi-threading scenarios)."
    pd->events &= (~ev);

    struct epoll_event epev;
    epev.events = pd->events;
    epev.data.ptr = pd;
    if (::epoll_ctl(this->efd, EPOLL_CTL_MOD, fd, &epev) == 0)
        return 0;
    pd->events |= ev; // recover
    return -1;
}
void evpoll::run() {
    int i = 0, nfds = 0, efd = this->efd;
    poll_desc *pd  = nullptr;
    ev_handler *eh = nullptr;
    struct epoll_event *ev_itor = nullptr;
    const int evpoll_ready_events_size = 128;
    struct epoll_event ready_events[evpoll_ready_events_size]; // stack variable

    while (true) {
        nfds = ::epoll_wait(efd, ready_events, evpoll_ready_events_size, -1);
        for (i = 0; i < nfds; ++i) {
            ev_itor = ready_events + i;
            pd = (poll_desc*)(ev_itor->data.ptr);

            // EPOLLHUP refer to man 2 epoll_ctl
            if (ev_itor->events & (EPOLLHUP|EPOLLERR)) {
                eh = pd->eh;
                this->remove(pd->fd, ev_handler::ev_all); // MUST before on_close
                eh->on_close();
                continue;
            }

            // MUST before EPOLLIN (e.g. connect)
            if ((ev_itor->events & (EPOLLOUT)) && (pd->eh->on_write() == false)) {
                eh = pd->eh;
                this->remove(pd->fd, ev_handler::ev_all); // MUST before on_close
                eh->on_close();
                continue;
            }

            if ((ev_itor->events & (EPOLLIN)) && (pd->eh->on_read() == false)) {
                eh = pd->eh;
                this->remove(pd->fd, ev_handler::ev_all); // MUST before on_close
                eh->on_close();
                continue;
            }
        } // end of `for i < nfds'
    }
}
