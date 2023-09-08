#ifndef NBX_EVPOLL_H_
#define NBX_EVPOLL_H_

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
private:
    int efd = -1;
    std::atomic<int> active_num = {0};
    poll_desc_map *poll_descs = nullptr;
};

#endif // NBX_EVPOLL_H_
