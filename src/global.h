#ifndef NBX_REACTOR_H_
#define NBX_REACTOR_H_

// Forward declarations
class reactor;

class g {
public:
    static int pid;
    static int shutdown_child_pid;
    static int child_pid;
    static reactor *conn_reactor;
    static reactor *g_reactor;
};

#endif // NBX_REACTOR_H_
