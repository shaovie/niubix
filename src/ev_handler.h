#ifndef EV_HANDLER_H_
#define EV_HANDLER_H_

#include <cstddef>
#include <unistd.h>
#include <sys/epoll.h>

// Forward declarations
class worker; 
class reactor; 
class timer_item; 

class ev_handler
{
public:
  enum {
    ev_read       = EPOLLIN  | EPOLLRDHUP,
    ev_write      = EPOLLOUT | EPOLLRDHUP,
    ev_accept     = EPOLLIN  | EPOLLRDHUP,
    ev_connect    = EPOLLIN  | EPOLLOUT | EPOLLRDHUP,
    ev_all        = ev_read|ev_write|ev_accept|ev_connect,
  };
  enum {
      err_connect_timeout = 1,
      err_connect_fail    = 2, // hostunreach, connrefused, connreset
  };
public:
  virtual ~ev_handler() {}

  virtual bool on_open()  { return false; }

  // the on_close(const int ev) will be called if below on_read/on_write return false. 
  virtual bool on_read()  { return false; }

  virtual bool on_write() { return false; }

  virtual void on_connect_fail(const int /*err*/) { }

  virtual bool on_timeout(const int64_t /*now*/) { return false; }

  // called by on_open/on_read/on_write return false, or ev trigger EPOLLHUP | EPOLLERR
  virtual void on_close() { }

  virtual void set_remote_addr(const struct sockaddr * /*addr*/, const socklen_t /*socklen*/) { };

  inline virtual int get_fd() const { return this->fd; }
  inline virtual void set_fd(const int v) { this->fd = v; }

  inline worker *get_worker() const { return this->wrker; }
  inline reactor *get_reactor(void) const { return this->rct; }

  void destroy() {
      if (this->fd != -1) {
          ::close(this->fd);
          this->fd = -1;
      }
      this->wrker = nullptr;
      this->rct   = nullptr;
      this->ti    = nullptr;
  }
private:
  virtual void sync_ordered_send(async_send_buf &) { }

  inline void set_worker(worker *p) { this->wrker = p; }

  inline void set_reactor(reactor *r) { this->rct = r; }

  inline timer_item *get_timer() { return this->ti; }
  inline void set_timer(timer_item *t) { this->ti = t; }
protected:
  ev_handler() = default;

private:
  int fd = -1;
  worker *wrker = nullptr;
  reactor *rct = nullptr;
  timer_item *ti = nullptr;
};

#endif // EV_HANDLER_H_
