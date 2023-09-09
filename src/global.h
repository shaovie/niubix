#ifndef NBX_GLOBAL_H_
#define NBX_GLOBAL_H_

// Forward declarations
class worker;
class leader;
class conf;

class g {
public:
    static int init(const conf *cf);

    static int pid;
    static int shutdown_child_pid;
    static int child_pid;
    static worker *main_worker;
    static leader *g_leader;
};

#endif // NBX_GLOBAL_H_
