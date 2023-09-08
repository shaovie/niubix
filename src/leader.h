#ifndef NBX_LEADER_H_
#define NBX_LEADER_H_

#include <cstdint>
#include <atomic>

// Forward declarations
class worker;
class ev_handler; 
class timer_shared; 

class leader {
public:
    leader() = default;

    //= run
    int  open(const options &opt);
    void run(const bool join = true);
    int  get_worker_num() const { return this->worker_num; }
    void close_all();
    void worker_online()  { this->active_worker_num.fetch_add(1, std::memory_order_relaxed); }
    void worker_offline() { this->active_worker_num.fetch_sub(1, std::memory_order_relaxed); }

    //= event
    int add_ev(ev_handler *eh, const int fd, const uint32_t events);
    int append_ev(const int fd, const uint32_t events);
    int remove_ev(const int fd, const uint32_t events);
    
private:
    int worker_num = 0;
    std::atomic<int> active_worker_num = {0};
    worker *workers = nullptr;
};

#endif // NBX_LEADER_H_
