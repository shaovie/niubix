#include "log.h"
#include "app.h"
#include "conf.h"
#include "global.h"
#include "worker.h"
#include "leader.h"

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/resource.h>

static int g_argc = 0;
static char **g_argv = nullptr;
static char *master_log_path = nullptr;

static char *fmttime() {
    struct tm ttm;
    static char time_buf[32] = {0};
    time_t t = ::time(nullptr);
    ::localtime_r(&t, &ttm);
    ::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &ttm);
    return time_buf;
}
static void master_log(const char *format, ...) {
  va_list argptr;
  ::va_start(argptr, format);
  FILE *fp = ::fopen(master_log_path, "a+");
  if (fp == nullptr) {
      ::va_end(argptr);
      return;
  }
  ::vfprintf(fp, format, argptr);
  ::va_end(argptr);
  ::fclose(fp);
}
static int set_max_fds(const int maxfds) {
  struct rlimit limit;
  limit.rlim_cur = maxfds;
  limit.rlim_max = maxfds;
  if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
      fprintf(stderr, "niubix: set RLIMIT_NOFILE fail! %s\n", strerror(errno));
      return -1;
  }
  return 0;
}
static int output_pid(const char *file_name) {
  char bf[32] = {0};
  int len = ::snprintf(bf, sizeof(bf), "%d", g::pid);
  int fd = ::open(file_name, O_CREAT|O_WRONLY, 0777);
  if (fd == -1) {
      master_log("%s write pid file failed! [%s]!\n", fmttime(), strerror(errno));
      return -1;
  }
  if (::flock(fd, LOCK_EX|LOCK_NB) != 0) {
      master_log("%s another niubix running!\n", fmttime()); 
      ::close(fd);
      ::exit(1);
  }
  if (::ftruncate(fd, 0) == -1) {
      master_log("%s write pid file ftruncate failed! [%s]!\n", fmttime(), strerror(errno));
      ::close(fd);
      return -1;
  }
  if (::write(fd, bf, len) == -1) {
      master_log("%s write pid file write failed! [%s]!\n", fmttime(), strerror(errno));
      ::close(fd);
      return -1;
  }
  return 0;
}
static void daemon() {
    int pid = ::fork();
    if (pid == -1) { // error
        fprintf(stderr, "niubix: fork failed! %s\n", strerror(errno));
        ::exit(1);
    } else if (pid > 0)
        ::exit(0);

    ::setsid();
    ::umask(0);
    int fd = ::open("/dev/null", O_RDWR);
    ::dup2(fd, 0);
    ::dup2(fd, 1);
    ::dup2(fd, 2);
    if (fd > 2)
        ::close(fd);
}
// 1. stop   niubix `kill -TERM master_pid`
// 2. reload niubix `kill -HUP  master_pid`
static pid_t exec_worker() {
    char **argv = new char *[g_argc + 4];
    for (int n = 0; n < g_argc; ++n)
        argv[n] = ::strdup(g_argv[n]);
    
    char pid_buf[16] = {0};
    ::snprintf(pid_buf, sizeof(pid_buf), "%d", g::pid);
    argv[g_argc] = ::strdup("-m");
    argv[g_argc + 1] = ::strdup(pid_buf);
    argv[g_argc + 2] = ::strdup("worker");
    argv[g_argc + 3] = nullptr;

    int pid = ::fork();
    if (pid == 0) { // child
        int fd = ::open("/dev/null", O_RDWR);
        for (int i = 0; i <= fd; ++i)
            ::close(i);
        ::execv(argv[0], argv);
    }
    for (int n = 0; n < g_argc; ++n)
        ::free(argv[n]);
    delete[] argv;
    return pid;
}
static void reload_worker(int) {
    if (g::shutdown_child_pid > 0) {
        master_log("%s reload err: child:%d is shutting down\n", fmttime(), g::shutdown_child_pid);
        return ;
    }
    if (g::child_pid == 0) {
        master_log("%s reload err: child pid is 0\n", fmttime());
        return ;
    }
    g::shutdown_child_pid = g::child_pid;
    master_log("%s master recv reload signal. execv worker and wait new worker start ok\n", fmttime());
    int pid = exec_worker();
    if (pid > 0)
        g::child_pid = pid;
}
static void master_shutdown(int) {
    ::kill(g::child_pid, SIGUSR2);
    master_log("%s master recv shutdown signal, kill child:%d and self exit normally.\n",
        fmttime(), g::child_pid);
    ::exit(0);
}
static void new_worker_start_ok(int);
static void master_run(conf *cf) {
    g::pid = ::getpid();
    if (output_pid(cf->pid_file) != 0)
        ::exit(1);
    
    master_log("\n%s master run %d\n", fmttime(), g::pid);
    ::signal(SIGUSR1, new_worker_start_ok);
    ::signal(SIGHUP,  reload_worker);
    ::signal(SIGTERM, master_shutdown);

    int status = 0;
    time_t exception_exit_time = 0;
    while (1) {
        int pid = exec_worker();
        g::child_pid = pid;
        master_log("%s new child %d\n", fmttime(), pid);
        
        // parent
        while (1) {
            int retpid = ::waitpid(-1, &status, 0);
            if (retpid == -1) {
                if (errno == EINTR)
                    continue ;
                master_log("%s waitpid return error. master exit(1) [%s]\n", fmttime(), strerror(errno));
                ::exit(1);
            }
            if (retpid == g::shutdown_child_pid) {
                master_log("%s child:%d shutdown\n", fmttime(), retpid);
                g::shutdown_child_pid = 0;
                continue;
            }
            if (WIFEXITED(status)) {
                master_log("%s child:%d exit normally, then master exit\n", fmttime(), retpid);
                ::exit(0); // worker exit normally.
            }
            if (WIFSIGNALED(status)) {
                master_log("%s child:%d exit by signal %d\n", fmttime(), retpid, WTERMSIG(status));
                auto now = time(nullptr);
                if (now - exception_exit_time < 10) {
                    master_log("%s child:%d exit by signal in %ld seconds, master exit\n",
                        fmttime(), retpid, now - exception_exit_time);
                    ::exit(0); // worker exit exception.
                }
                exception_exit_time = now;
                // goto fork
            }
            break;
        }
        ::usleep(50*1000); // 50msec
    }
}
static void new_worker_start_ok(int) {
    if (g::shutdown_child_pid == 0)
        return ;
    master_log("%s master recv new worker start ok signal, then shutdown oldpid:%d\n",
        fmttime(), g::shutdown_child_pid);
    ::kill(g::shutdown_child_pid, SIGUSR1);
}
static bool worker_shutdowning = false;
static void let_worker_shutdown(int) {
    log::info("worker:%d recv shutdown signal", g::pid);
    if (worker_shutdowning == true)
        return ;
    worker_shutdowning = true;
    g::let_worker_shutdown();
}
static void print_usage() {
    printf("usage: niubix [options] [-]\n");
    printf("  -c /path/to/niubix.json\n");
    printf("  -t test conf\n");
    printf("  -v version info\n");
    printf("  -h help info\n");
    ::exit(0);
}
int main(int argc, char *argv[]) {
    if (argc < 2 || argv[1][0] != '-')
        print_usage();

    g_argc = argc;
    g_argv = argv;
    int c = -1;
    extern char *optarg;
    char *conf_path = nullptr;
    int master_pid = 0;
    bool test_conf = false;
    while ((c = getopt(argc, argv, ":c:tvhm:")) != -1) {
        switch (c) {
        case 'v':
            printf("niubix v%s\n", "0.0.1");
            return 0;
        case 'm':
            master_pid = ::atoi(optarg);
            break;
        case 't':
            test_conf = true;
            break;
        case 'c':
            conf_path = optarg;
            break;
        default:
            print_usage();
        }
    }

    conf *cf = new conf(); 
    if (cf->load(conf_path) != 0)
        ::exit(1);
    if (test_conf == true) {
        delete cf;
        printf("niubix conf:%s test passed!\n", conf_path);
        ::exit(0);
    }

    if (cf->max_fds > 0 && set_max_fds(cf->max_fds) != 0)
        ::exit(1);

    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGINT,  SIG_IGN);

    if (master_pid == 0) {
        master_log_path = cf->master_log;
        daemon(); // master daemon
        
        master_run(cf);
        ::exit(0);
    }

    // child process
    g::pid = ::getpid();
    ::signal(SIGUSR1, let_worker_shutdown);
    if (log::open(cf->log_dir, "niubix", cf->log_level) != 0)
        ::exit(1);
    
    if (g::init(cf) != 0) // global init
        ::exit(1);

    if (app::run_all(cf) != 0)
        ::exit(1);

    g::g_leader->run(false);

    log::info("niubix worker:%d start", g::pid);
    ::kill(master_pid, SIGUSR1); // Notify the master that the old worker can be shutdown.

    g::main_worker->run();
    return 0;
}
