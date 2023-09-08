#ifndef NBX_LOG_H_
#define NBX_LOG_H_

// Forward declarations
class log_impl; 

class log {
public:
    enum {
        SHUTDOWN  = 1L << 0,
        DEBUG     = 1L << 1,
        INFO      = 1L << 2,
        WARN      = 1L << 3,
        ERROR     = 1L << 4,
        ALLS      = DEBUG | INFO | WARN | ERROR,
    };
    static int open(const char *dir, const char *filename_prefix, const char *lt);
    static int reset(const char *dir, const char *filename_prefix, const int lt);
    static void close();

    static void debug(const char *format, ...);
    static void info(const char *format, ...);
    static void warn(const char *format, ...);
    static void error(const char *format, ...);
private:
    static log_impl *log_debug;
    static log_impl *log_info;
    static log_impl *log_warn;
    static log_impl *log_error;
};

#endif // NBX_LOG_H_
