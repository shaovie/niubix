#ifndef NBX_CONF_H_
#define NBX_CONF_H_

#include "defines.h"

class conf {
public:
    conf() = default;

    int load(const char *path);
public:
    bool reuse_addr = true;
    bool reuse_port = true;
    int  rcvbuf_size = 0;
    int  backlog = 128; // for listen
    int  set_cpu_affinity = true;
    int  worker_num = 0;
    int  max_fds = 0;
    int  worker_io_buf_size = 32*1024;
    int  timer_init_size = 1024*4;
    char pid_file[MAX_FILE_NAME_LENGTH] = {0};
    char log_dir[MAX_FILE_NAME_LENGTH] = {0};
    char log_level[MAX_FILE_NAME_LENGTH] = {0};
    char master_log[MAX_FILE_NAME_LENGTH] = {0};
};

#endif // NBX_CONF_H_

