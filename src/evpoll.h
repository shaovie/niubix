#ifndef NBX_EVPOLL_H_
#define NBX_EVPOLL_H_

#include "ringq.h"
#include "task_in_worker.h"

#include <atomic>
#include <cstdint>

// Forward declarations
class ev_handler; 
class poll_desc_map;

class evpoll {
public:
    evpoll() = default;

    int open(const int max_fds);

    int add(ev_handler *eh, const int fd, const uint32_t ev);

    int append(const int fd, const uint32_t ev);

    int remove(const int fd, const uint32_t ev);

    void run();

    void push_task(const task_in_worker &t) { this->taskq->push_back(t); }
private:
    int efd = -1;
    std::atomic<int> active_num = {0};
    poll_desc_map *poll_descs = nullptr;
    ringq<task_in_worker> *taskq = nullptr;
};

#endif // NBX_EVPOLL_H_
