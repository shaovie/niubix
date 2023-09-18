#ifndef NBX_GLOBAL_H_
#define NBX_GLOBAL_H_

#include <cstdint>

// Forward declarations
class worker;
class leader;
class conf;

class g {
public:
    static int init(const conf *cf);
    static void let_worker_shutdown();

    static int pid;
    static int shutdown_child_pid;
    static int child_pid;
    static int64_t worker_start_time; // second
    static worker *main_worker;
    static leader *g_leader;
};

#endif // NBX_GLOBAL_H_
