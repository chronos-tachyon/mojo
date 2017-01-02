// base/cleanup.h - RAII class to run code upon leaving a scope
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_CLEANUP_H
#define BASE_CLEANUP_H

#include <utility>

namespace base {

// Cleanup is a RAII class for running a void() functor at the end of a scope.
//
// Typical usage:
//
//    void myfunction() {
//      int fd = open("/path/to/file", O_RDONLY);
//      auto cleanup = base::cleanup([fd] {
//        close(fd);
//      });
//
//      // Now the fd will be closed when we exit myfunction,
//      // even if an exception is thrown.
//
//      if (early_return) return;           // this is fine
//      if (error_cond) throw exception();  // so is this
//      ...;                                // code using fd
//    }
//
template <typename F>
class Cleanup {
 public:
  // Construct a Cleanup object from a functor.
  Cleanup(F f) noexcept : func_(std::move(f)), need_(true) {}

  // Cleanup objects are not copyable.
  Cleanup(const Cleanup&) = delete;
  Cleanup& operator=(const Cleanup&) = delete;

  // Cleanup supports move construction and assignment.
  Cleanup(Cleanup&& x) noexcept : func_(std::move(x.func_)), need_(x.need_) {
    x.need_ = false;
  }

  Cleanup& operator=(Cleanup&& x) noexcept {
    func_ = std::move(x.func_);
    need_ = std::move(x.need_);
    x.need_ = false;
    return *this;
  }

  // Cleanup runs its functor at destruction time.
  ~Cleanup() noexcept(noexcept(std::declval<F>()())) { run(); }

  void swap(Cleanup& x) noexcept {
    using std::swap;
    swap(func_, x.func_);
    swap(need_, x.need_);
  }

  // Checks if this Cleanup still needs to run.
  explicit operator bool() const noexcept { return need_; }

  // Cancels this Cleanup, i.e. marks it as not needing to run.
  void cancel() noexcept { need_ = false; }

  // Forces this Cleanup to run now.  Idempotent.
  void run() noexcept(noexcept(std::declval<F>()())) {
    if (need_) {
      need_ = false;
      func_();
    }
  }

 private:
  F func_;
  bool need_;
};

template <typename F>
void swap(Cleanup<F>& a, Cleanup<F>& b) noexcept { a.swap(b); }

// Function for constructing Cleanup objects.  Supports type inference.
template <typename F>
inline Cleanup<F> cleanup(F f) {
  return Cleanup<F>(std::move(f));
}

}  // namespace base

#endif  // BASE_CLEANUP_H
