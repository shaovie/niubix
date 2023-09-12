#include "log.h"
#include "defines.h"

#include <mutex>
#include <atomic>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

class log_impl {
public:
    log_impl(const char *ln, const char *dir, const char *fname, const int lt)
        : log_types(lt),
        log_name(ln),
        dir(dir)
    {
        if (::strlen(fname) == 0)
            this->filename = ln;
        else
            this->filename = std::string(fname) + "-" + std::string(ln);
    }

    inline int get_log_types() { return this->log_types.load(std::memory_order_relaxed); }
    void log(const char *format, va_list &va_ptr);
    void close();
private:
    int new_file(const int year, const int month, const int mday);
    int format(const char *format, va_list &va_ptr);
    int output(const char *r, const int len);
private:
	int fd = -1;
	int new_file_year  = 0; 
	int new_file_month = 0;
	int new_file_mday  = 0;
    std::atomic<int> log_types = {0};
    char cached_date[11] = {0};
    std::string log_name;
    std::string filename;
    std::string dir;
    std::mutex mtx;
};
void log_impl::close() {
    std::lock_guard<std::mutex> g(this->mtx);
    if (!this->dir.empty() && this->fd != -1) {
        ::close(this->fd);
        this->fd = -1;
    }
}
void log_impl::log(const char *format, va_list &va_ptr) {
    int len = 0;
    struct timeval now;
    ::gettimeofday(&now, nullptr);
    struct tm ttm;
    char time_buf[13] = {0};
    int msec = (int)((now.tv_usec + 999) / 1000);
    if (msec > 999) {
        ++(now.tv_sec);
        msec -= 1000;
    }
    ::localtime_r(&(now.tv_sec), &ttm);
    ::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &ttm);

    std::lock_guard<std::mutex> g(this->mtx);

    if (this->new_file_year != ttm.tm_year + 1900
        || this->new_file_month != ttm.tm_mon
        || this->new_file_mday != ttm.tm_mday)
        if (this->new_file(ttm.tm_year + 1900, ttm.tm_mon, ttm.tm_mday) != 0)
            return ;

    char log_record[MAX_LENGTH_OF_ONE_LOG + 1]; // stack variable
    int ret = ::snprintf(log_record,
        MAX_LENGTH_OF_ONE_LOG,
        "%s %s.%03d %ld ",
        this->cached_date,
        time_buf,
        msec, pthread_self());
    if (ret >= MAX_LENGTH_OF_ONE_LOG)
        ret = MAX_LENGTH_OF_ONE_LOG - 1;
    len += ret;
    if (this->fd == STDOUT_FILENO) {
        ::strncat(log_record + len, this->log_name.c_str(), MAX_LENGTH_OF_ONE_LOG - len);
        len += this->log_name.length();
        if (MAX_LENGTH_OF_ONE_LOG - len < 1)
            return ;
        ::strncat(log_record + len, " ", MAX_LENGTH_OF_ONE_LOG - len);
        len += 1;
    }
    if (len < MAX_LENGTH_OF_ONE_LOG) {
        ret = ::vsnprintf(log_record + len,
            MAX_LENGTH_OF_ONE_LOG - len,
            format,
            va_ptr);
        /**
         * check overflow or not
         * Note : snprintf and vnprintf return value is the number of characters
         * (not including the trailing ’\0’) which would have been  written  to
         * the  final  string  if enough space had been available.
         */
        if (ret >= (MAX_LENGTH_OF_ONE_LOG - len))
            ret = MAX_LENGTH_OF_ONE_LOG - len - 1;
        // vsnprintf return the length of <va_ptr> actual
        len += ret;
    }
    log_record[len] = '\n';
    log_record[len + 1] = '\0';
    this->output(log_record, len + 1);
}
int log_impl::output(const char *record, const int len) {
    do {
        if (::write(this->fd, record, len) <= 0) {
            if (errno == ENOSPC)
                return 0;
            else if (errno == EINTR)
                continue;
            return -1;
        }
    } while (false);
    return 0;
}
int log_impl::new_file(const int year, const int month, const int mday) {
    if (this->dir.empty()) {
        this->fd = STDOUT_FILENO;
    } else {
        if (this->fd != -1) {
            ::close(this->fd);
            this->fd = -1;
        }
        char fname[MAX_FILE_NAME_LENGTH + 1] = {0};
        ::snprintf(fname, sizeof(fname), "%s/%s-%d-%02d-%02d.log",
            this->dir.c_str(), this->filename.c_str(),
            year, month, mday);
        do {
            int fd = ::open(fname, O_CREAT | O_WRONLY | O_APPEND, 0644);
            if (fd == -1) {
                if (errno == EINTR)
                    continue ;
                return -1;
            }
            this->fd = fd;
        } while (false);
    }
    this->new_file_year = year;
    this->new_file_month = month;
    this->new_file_mday = mday;
    ::snprintf(this->cached_date, sizeof(this->cached_date), "%d-%02d-%02d", year, month, mday);
    return 0;
}
//= log
log_impl* log::log_debug = nullptr;
log_impl* log::log_info  = nullptr;
log_impl* log::log_warn  = nullptr;
log_impl* log::log_error = nullptr;
int log::open(const char *dir, const char *filename_prefix, const char *lt) {
    if (dir != nullptr && ::strlen(dir) > 0 && ::mkdir(dir, 0755) == -1) {
        if (errno != EEXIST) {
            fprintf(stderr, "niubix: log mkdir(%s) error %s\n", dir, strerror(errno));
            return -1;
        }
    }
    char *tok_p = NULL;
    char *token = NULL;
    char bf[128] = {0};
    ::strncpy(bf, lt, sizeof(bf));
    bf[sizeof(bf) - 1] = '\0';
    int l_type = 0;
    for (token = ::strtok_r(bf, "|", &tok_p); 
         token != NULL;
         token = ::strtok_r(NULL, ",", &tok_p))
    {
        if (::strcmp(token, "shutdown") == 0) {
          l_type |= SHUTDOWN;
          break;
        }
        if (::strcmp(token, "all") == 0) {
          l_type |= ALLS;
          break;
        }
        if (::strcmp(token, "debug") == 0)
          l_type |= DEBUG;
        else if (::strcmp(token, "info") == 0)
          l_type |= INFO;
        else if (::strcmp(token, "warn") == 0)
          l_type |= WARN;
        else if (::strcmp(token, "error") == 0)
          l_type |= ERROR;
    }
    if (l_type & SHUTDOWN)
        l_type = SHUTDOWN;

    log::log_debug = new log_impl("debug", dir, filename_prefix, l_type);
    log::log_info  = new log_impl("info",  dir, filename_prefix, l_type);
    log::log_warn  = new log_impl("warn",  dir, filename_prefix, l_type);
    log::log_error = new log_impl("error", dir, filename_prefix, l_type);
    return 0;
}
# define SHORT_CODE(T, LT)  int lt = log::log_##T->get_log_types();\
                            if (!(lt & LT) || (lt & SHUTDOWN))     \
                                 return ;                          \
                             va_list va; va_start(va, format);     \
                             log::log_##T->log(format, va);        \
                             va_end(va)

void log::debug(const char *format, ...)    { SHORT_CODE(debug, DEBUG); }
void log::info(const char *format,  ...)    { SHORT_CODE(info,  INFO);  }
void log::warn(const char *format,  ...)    { SHORT_CODE(warn,  WARN);  }
void log::error(const char *format, ...)    { SHORT_CODE(error, ERROR); }
void log::close() {
    log::log_debug->close();
    log::log_info->close();
    log::log_warn->close();
    log::log_error->close();
}
