// net/conn.h - Abstraction for network connections
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_CONN_H
#define NET_CONN_H

#include <sys/time.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/result.h"
#include "io/reader.h"
#include "io/writer.h"
#include "net/addr.h"
#include "net/options.h"
#include "net/sockopt.h"

namespace net {

// ConnImpl is the abstract base class for connected sockets.
class ConnImpl {
 protected:
  ConnImpl() noexcept = default;

 public:
  // ConnImpl is neither copyable nor moveable.
  ConnImpl(const ConnImpl&) = delete;
  ConnImpl(ConnImpl&&) = delete;
  ConnImpl& operator=(const ConnImpl&) = delete;
  ConnImpl& operator=(ConnImpl&&) = delete;
  virtual ~ConnImpl() noexcept = default;

  // Returns the address of this end of the socket.
  virtual Addr local_addr() const = 0;

  // Returns the address of the remote end of the socket.
  virtual Addr remote_addr() const = 0;

  // Returns an io::Reader which receives data from the remote end.
  //
  // - |reader().close()| MUST half-close the socket in the read direction.
  //   If read half-closing is not implemented, then |reader().close()| MUST
  //   have no effect.
  //
  // - Multiple calls to |reader()| MUST return the same io::Reader object.
  //
  virtual io::Reader reader() = 0;

  // Returns an io::Writer which sends data to the remote end.
  //
  // - |writer().close()| MUST half-close the socket in the write direction.
  //   If write half-closing is not implemented, then |writer().close()| MUST
  //   have no effect.
  //
  // - Multiple calls to |writer()| MUST return the same io::Writer object.
  //
  virtual io::Writer writer() = 0;

  // Fully closes the socket.
  virtual void close(event::Task* task, const base::Options& opts) = 0;

  // Retrieves the value of a socket option.
  virtual void get_option(event::Task* task, SockOpt opt, void* optval,
                          unsigned int* optlen,
                          const base::Options& opts) const = 0;

  // Assigns the value of a socket option.
  virtual void set_option(event::Task* task, SockOpt opt, const void* optval,
                          unsigned int optlen, const base::Options& opts) = 0;
};

// Conn is a handle to a connected socket.
//
// A "connected socket", in this context, is a bi-directional I/O stream.
//
// A Conn typically points at a socket, and therefore exists in the "non-empty"
// state.  In contrast, a Conn without a socket exists in the "empty" state.  A
// default-constructed Conn is empty, as is a Conn on which the |reset()|
// method is called.
//
// Sockets are reference counted.  When the last Conn referencing a socket is
// destroyed or becomes empty, then the socket is closed.
//
// Most methods are illegal to call on an empty Conn.
//
class Conn {
 public:
  using Pointer = std::shared_ptr<ConnImpl>;

  // Conn is constructible from an implementation.
  Conn(Pointer ptr) noexcept : ptr_(std::move(ptr)) {}

  // Conn is default constructible, starting in the empty state.
  Conn() noexcept = default;

  // Conn is copyable and moveable.
  // - These copy or move the handle, not the socket itself.
  Conn(const Conn&) noexcept = default;
  Conn(Conn&&) noexcept = default;
  Conn& operator=(const Conn&) noexcept = default;
  Conn& operator=(Conn&&) noexcept = default;

  // Resets this Conn to the empty state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this Conn with another.
  void swap(Conn& x) noexcept { ptr_.swap(x.ptr_); }

  // Returns true iff this Conn is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Conn is non-empty.
  void assert_valid() const;

  // Returns this Conn's socket implementation.
  const Pointer& implementation() const { return ptr_; }
  Pointer& implementation() { return ptr_; }

  // Returns the address of this end of the socket.
  Addr local_addr() const {
    assert_valid();
    return ptr_->local_addr();
  }

  // Returns the address of the remote end of the socket.
  Addr remote_addr() const {
    assert_valid();
    return ptr_->remote_addr();
  }

  // Returns an io::Reader which receives data from the remote end.
  // Closing the io::Reader half-closes the socket in the read direction.
  io::Reader reader() const {
    assert_valid();
    return ptr_->reader();
  }

  // Returns an io::Writer which sends data to the remote end.
  // Closing the io::Writer half-closes the socket in the write direction.
  io::Writer writer() const {
    assert_valid();
    return ptr_->writer();
  }

  // Fully closes the socket.
  void close(event::Task* task,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->close(task, opts);
  }

  // Retrieves the value of a socket option.
  void get_option(event::Task* task, SockOpt opt, void* optval,
                  unsigned int* optlen,
                  const base::Options& opts = base::default_options()) const;
  void get_int_option(
      event::Task* task, SockOpt opt, int* value,
      const base::Options& opts = base::default_options()) const;
  void get_tv_option(event::Task* task, SockOpt opt, struct timeval* value,
                     const base::Options& opts = base::default_options()) const;

  // Assigns the value of a socket option.
  void set_option(event::Task* task, SockOpt opt, const void* optval,
                  unsigned int optlen,
                  const base::Options& opts = base::default_options()) const;
  void set_int_option(
      event::Task* task, SockOpt opt, int value,
      const base::Options& opts = base::default_options()) const;
  void set_tv_option(event::Task* task, SockOpt opt, struct timeval value,
                     const base::Options& opts = base::default_options()) const;

  // Synchronous versions of the above.
  base::Result close(const base::Options& opts = base::default_options()) const;
  base::Result get_option(
      SockOpt opt, void* optval, unsigned int* optlen,
      const base::Options& opts = base::default_options()) const;
  base::Result get_int_option(
      SockOpt opt, int* value,
      const base::Options& opts = base::default_options()) const;
  base::Result get_tv_option(
      SockOpt opt, struct timeval* value,
      const base::Options& opts = base::default_options()) const;
  base::Result set_option(
      SockOpt opt, const void* optval, unsigned int optlen,
      const base::Options& opts = base::default_options()) const;
  base::Result set_int_option(
      SockOpt opt, int value,
      const base::Options& opts = base::default_options()) const;
  base::Result set_tv_option(
      SockOpt opt, struct timeval value,
      const base::Options& opts = base::default_options()) const;

 private:
  Pointer ptr_;
};

// Conn objects are swappable.
inline void swap(Conn& a, Conn& b) noexcept { a.swap(b); }

// Conn objects are comparable for equality.
inline bool operator==(const Conn& a, const Conn& b) noexcept {
  return a.implementation() == b.implementation();
}
inline bool operator!=(const Conn& a, const Conn& b) noexcept {
  return !(a == b);
}

// ListenConn implementations take an AcceptFn at construction time.
// The AcceptFn will be called once for each new connected socket.
using AcceptFn = std::function<void(Conn)>;

// ListenConnImpl is the abstract base class for listening sockets.
class ListenConnImpl {
 protected:
  ListenConnImpl() noexcept = default;

 public:
  // ListenConnImpl is neither copyable nor moveable.
  ListenConnImpl(const ListenConnImpl&) = delete;
  ListenConnImpl(ListenConnImpl&&) = delete;
  ListenConnImpl& operator=(const ListenConnImpl&) = delete;
  ListenConnImpl& operator=(ListenConnImpl&&) = delete;

  virtual ~ListenConnImpl() noexcept = default;

  // Returns the address to which this socket is bound.
  virtual Addr listen_addr() const = 0;

  // Starts accepting new connected sockets from peers.
  // - MUST be idempotent
  virtual void start(event::Task* task, const base::Options& opts) = 0;

  // Stops accepting new connected sockets from peers.
  // - MUST be idempotent
  // - MUST NOT release the bound address
  virtual void stop(event::Task* task, const base::Options& opts) = 0;

  // Fully closes the socket.
  virtual void close(event::Task* task, const base::Options& opts) = 0;

  // Retrieves the value of a socket option.
  virtual void get_option(event::Task* task, SockOpt opt, void* optval,
                          unsigned int* optlen,
                          const base::Options& opts) const = 0;

  // Assigns the value of a socket option.
  virtual void set_option(event::Task* task, SockOpt opt, const void* optval,
                          unsigned int optlen, const base::Options& opts) = 0;
};

// ListenConn is a handle to a listening socket.
//
// A "listening socket", in this context, is a source of new connected sockets.
// It is bound to some local address, allowing peers to find it and connect.
//
// A ListenConn typically points at a socket, and therefore exists in the
// "non-empty" state.  In contrast, a ListenConn without a socket exists in the
// "empty" state.  A default-constructed ListenConn is empty, as is a
// ListenConn on which the |reset()| method is called.
//
// Sockets are reference counted.  When the last Conn referencing a socket is
// destroyed or becomes empty, then the socket is closed.
//
class ListenConn {
 public:
  using Pointer = std::shared_ptr<ListenConnImpl>;

  // ListenConn is constructible from an implementation.
  ListenConn(Pointer ptr) noexcept : ptr_(std::move(ptr)) {}

  // ListenConn is default constructible, starting in the empty state.
  ListenConn() noexcept = default;

  // ListenConn is copyable and moveable.
  // - These copy or move the handle, not the socket itself.
  ListenConn(const ListenConn&) noexcept = default;
  ListenConn(ListenConn&&) noexcept = default;
  ListenConn& operator=(const ListenConn&) noexcept = default;
  ListenConn& operator=(ListenConn&&) noexcept = default;

  // Resets this ListenConn to the empty state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this ListenConn with another.
  void swap(ListenConn& x) noexcept { ptr_.swap(x.ptr_); }

  // Returns true iff this ListenConn is non-empty.
  explicit operator bool() const { return !!ptr_; }

  // Asserts that this ListenConn is non-empty.
  void assert_valid() const;

  // Returns this ListenConn's socket implementation.
  const Pointer& implementation() const { return ptr_; }
  Pointer& implementation() { return ptr_; }

  // Returns the address to which the socket is bound.
  Addr listen_addr() const {
    assert_valid();
    return ptr_->listen_addr();
  }

  // Starts accepting new connected sockets from peers.
  //
  // - Calling |start()| moves the socket to the accepting state.
  // - It is a no-op to call |start()| while already in the accepting state.
  //
  void start(event::Task* task,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->start(task, opts);
  }

  // Stops accepting new connected sockets from peers.
  //
  // - Calling |stop()| moves the socket to the non-accepting state.
  // - It is a no-op to call |stop()| while already in the non-accepting state.
  //
  // After this call, the socket is still bound and listening. However, the
  // socket will no longer call accept(2), and thus incoming connections will
  // backlog in the kernel.  (This describes the situation for native sockets.
  // Userspace protocols may differ in terminology, but not behavior.)
  //
  void stop(event::Task* task,
            const base::Options& opts = base::default_options()) const {
    assert_valid();
    return ptr_->stop(task, opts);
  }

  // Closes the listener.
  void close(event::Task* task,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    return ptr_->close(task, opts);
  }

  // Retrieves the value of a socket option.
  void get_option(event::Task* task, SockOpt opt, void* optval,
                  unsigned int* optlen,
                  const base::Options& opts = base::default_options()) const;
  void get_int_option(
      event::Task* task, SockOpt opt, int* value,
      const base::Options& opts = base::default_options()) const;
  void get_tv_option(event::Task* task, SockOpt opt, struct timeval* value,
                     const base::Options& opts = base::default_options()) const;

  // Assigns the value of a socket option.
  void set_option(event::Task* task, SockOpt opt, const void* optval,
                  unsigned int optlen,
                  const base::Options& opts = base::default_options()) const;
  void set_int_option(
      event::Task* task, SockOpt opt, int value,
      const base::Options& opts = base::default_options()) const;
  void set_tv_option(event::Task* task, SockOpt opt, struct timeval value,
                     const base::Options& opts = base::default_options()) const;

  // Synchronous versions of the above.
  base::Result start(const base::Options& opts = base::default_options()) const;
  base::Result stop(const base::Options& opts = base::default_options()) const;
  base::Result close(const base::Options& opts = base::default_options()) const;
  base::Result get_option(
      SockOpt opt, void* optval, unsigned int* optlen,
      const base::Options& opts = base::default_options()) const;
  base::Result get_int_option(
      SockOpt opt, int* value,
      const base::Options& opts = base::default_options()) const;
  base::Result get_tv_option(
      SockOpt opt, struct timeval* value,
      const base::Options& opts = base::default_options()) const;
  base::Result set_option(
      SockOpt opt, const void* optval, unsigned int optlen,
      const base::Options& opts = base::default_options()) const;
  base::Result set_int_option(
      SockOpt opt, int value,
      const base::Options& opts = base::default_options()) const;
  base::Result set_tv_option(
      SockOpt opt, struct timeval value,
      const base::Options& opts = base::default_options()) const;

 private:
  Pointer ptr_;
};

// ListenConn is swappable.
inline void swap(ListenConn& a, ListenConn& b) noexcept { a.swap(b); }

// ListenConn is comparable for equality.
inline bool operator==(const ListenConn& a, const ListenConn& b) noexcept {
  return a.implementation() == b.implementation();
}
inline bool operator!=(const ListenConn& a, const ListenConn& b) noexcept {
  return !(a == b);
}

}  // namespace net

#endif  // NET_CONN_H
