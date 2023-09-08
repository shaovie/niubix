#ifndef NBX_OPTIONS_H_
#define NBX_OPTIONS_H_

class options {
public:
    options() = default;

    // reactor option
    bool set_cpu_affinity = true;
    int worker_io_buf_size  = 256 * 1024; // for read & write sync i/o
    int worker_num = 0; // defaut equal cpu num
    int max_fds = 65535; // 整个进程内fds 最大数量，超过此数量会拒绝服务

    // timer option
    int timer_init_size = 1024*4;

    // acceptor option
    bool reuse_addr = true;
    bool reuse_port = false;
    int  rcvbuf_size = 0;
    int  backlog = 128; // for listen
};

#endif // NBX_OPTIONS_H_
