#ifndef NBX_GLOBAL_H_
#define NBX_GLOBAL_H_

#include <cstdint>

// Forward declarations
class worker;
class leader;
class conf;
class acceptor;

class g {
public:
    static int init(const conf *cf);
    static int init_ssl();
    static void let_worker_shutdown();

    static bool worker_shutdowning;
    static int pid;
    static int shutdown_child_pid;
    static int child_pid;
    static int master_pid;
    static int64_t worker_start_time; // second
    static const conf *cf;
    static acceptor *admin_acceptor;
    static worker *main_worker;
    static leader *g_leader;
};

#endif // NBX_GLOBAL_H_
