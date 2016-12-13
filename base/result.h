// base/result.h - Value type representing operation success or failure
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_RESULT_H
#define BASE_RESULT_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "base/concat.h"

namespace base {

// Result represents the success or failure of an operation.
// Failures are further categorized by the type of failure.
class Result {
 public:
  // Code represents the type of success/failure that a Result represents.
  enum class Code : uint8_t {
    // Success.
    OK = 0x00,

    // Failure of an unknown type, or whose type does not fit into these codes.
    UNKNOWN = 0x01,

    // Internal-only failure that should never be seen by the user.
    INTERNAL = 0x02,

    // The operation was cancelled before it could complete.
    CANCELLED = 0x03,

    // The world was in a state that was not compatible with the operation.
    // For example: attempting to close a file that isn't open.
    FAILED_PRECONDITION = 0x04,

    // The operation was unable to find the specified resource.
    // Subtype of: FAILED_PRECONDITION
    NOT_FOUND = 0x05,

    // The operation found that the specified resource already existed.
    // Subtype of: FAILED_PRECONDITION
    ALREADY_EXISTS = 0x06,

    // The operation found a resource of the wrong type.
    // For example: expected a directory, found a regular file.
    // Subtype of: FAILED_PRECONDITION
    WRONG_TYPE = 0x07,

    // The operation failed because the authenticated user is not authorized.
    // Subtype of: FAILED_PRECONDITION
    PERMISSION_DENIED = 0x08,

    // The operation failed because the user could not be authenticated.
    // Subtype of: FAILED_PRECONDITION
    UNAUTHENTICATED = 0x09,

    // The operation failed because of an argument that doesn't make sense.
    INVALID_ARGUMENT = 0x0a,

    // The operation failed because an argument was outside the valid range.
    // Subtype of: INVALID_ARGUMENT
    OUT_OF_RANGE = 0x0b,

    // The operation failed because the resource does not support it.
    NOT_IMPLEMENTED = 0x0c,

    // The operation failed because the resource was not available.
    // For example: cannot read a remote file because the network is down.
    UNAVAILABLE = 0x0d,

    // The operation failed because the system interrupted it.
    ABORTED = 0x0e,

    // The operation failed because a finite resource was already in use.
    // For example: too many open file handles.
    // For example: disk full.
    RESOURCE_EXHAUSTED = 0x0f,

    // The operation took so long that we gave up on it.
    // For example: write to remote file gives up (because network is down).
    DEADLINE_EXCEEDED = 0x10,

    // The operation failed because data was lost or corrupted.
    // For example: read from file fails due to bad checksum.
    DATA_LOSS = 0x11,

    // The operation failed because the end was reached prematurely.
    END_OF_FILE = 0x12,
  };

  // Returns the string representation of a Code.
  static const std::string& code_name(Code code) noexcept;

 private:
  struct Guts {
    Code code;
    int err_no;
    std::string message;

    Guts(Code code, int err_no, std::string&& message)
        : code(code), err_no(err_no), message(std::move(message)) {}
  };

  using NameMap = std::map<Code, std::string>;
  using MemoMap = std::map<Code, std::shared_ptr<const Guts>>;
  static std::mutex s_mu;
  static const NameMap* s_name_map;
  static const MemoMap* s_memo_map;

  static NameMap* build_name_map();
  static MemoMap* build_memo_map();

  static std::shared_ptr<const Guts> make(Code code, int err_no,
                                          std::string&& message);

 public:
  // Constructors for fixed Code values {{{

  template <typename... Args>
  static Result unknown(const Args&... args) {
    return Result(Code::UNKNOWN, concat(args...));
  }

  template <typename... Args>
  static Result internal(const Args&... args) {
    return Result(Code::INTERNAL, concat(args...));
  }

  template <typename... Args>
  static Result cancelled(const Args&... args) {
    return Result(Code::CANCELLED, concat(args...));
  }

  template <typename... Args>
  static Result failed_precondition(const Args&... args) {
    return Result(Code::FAILED_PRECONDITION, concat(args...));
  }

  template <typename... Args>
  static Result not_found(const Args&... args) {
    return Result(Code::NOT_FOUND, concat(args...));
  }

  template <typename... Args>
  static Result already_exists(const Args&... args) {
    return Result(Code::ALREADY_EXISTS, concat(args...));
  }

  template <typename... Args>
  static Result wrong_type(const Args&... args) {
    return Result(Code::WRONG_TYPE, concat(args...));
  }

  template <typename... Args>
  static Result permission_denied(const Args&... args) {
    return Result(Code::PERMISSION_DENIED, concat(args...));
  }

  template <typename... Args>
  static Result unauthenticated(const Args&... args) {
    return Result(Code::UNAUTHENTICATED, concat(args...));
  }

  template <typename... Args>
  static Result invalid_argument(const Args&... args) {
    return Result(Code::INVALID_ARGUMENT, concat(args...));
  }

  template <typename... Args>
  static Result out_of_range(const Args&... args) {
    return Result(Code::OUT_OF_RANGE, concat(args...));
  }

  template <typename... Args>
  static Result not_implemented(const Args&... args) {
    return Result(Code::NOT_IMPLEMENTED, concat(args...));
  }

  template <typename... Args>
  static Result unavailable(const Args&... args) {
    return Result(Code::UNAVAILABLE, concat(args...));
  }

  template <typename... Args>
  static Result aborted(const Args&... args) {
    return Result(Code::ABORTED, concat(args...));
  }

  template <typename... Args>
  static Result resource_exhausted(const Args&... args) {
    return Result(Code::RESOURCE_EXHAUSTED, concat(args...));
  }

  template <typename... Args>
  static Result deadline_exceeded(const Args&... args) {
    return Result(Code::DEADLINE_EXCEEDED, concat(args...));
  }

  template <typename... Args>
  static Result data_loss(const Args&... args) {
    return Result(Code::DATA_LOSS, concat(args...));
  }

  template <typename... Args>
  static Result eof(const Args&... args) {
    return Result(Code::END_OF_FILE, concat(args...));
  }

  // }}}
  // Constructors for errno-to-Result conversions {{{

  static Result from_errno(int err_no, std::string what);

  template <typename... Args>
  static Result from_errno(int err_no, const Args&... args) {
    return from_errno(err_no, concat(args...));
  }

  // }}}

  // Result is default constructible, copyable, and moveable.
  // The default-constructed value has code OK, message "", errno -1.
  Result() noexcept = default;
  Result(const Result&) noexcept = default;
  Result(Result&&) noexcept = default;
  Result& operator=(const Result&) noexcept = default;
  Result& operator=(Result&&) noexcept = default;

  Result(Code code) : guts_(make(code, -1, std::string())) {}
  Result(Code code, std::string message)
      : guts_(make(code, -1, std::move(message))) {}
  Result(Code code, std::string message, int err_no)
      : guts_(make(code, err_no, std::move(message))) {}

  Result(std::nullptr_t) noexcept : Result() {}
  Result& operator=(std::nullptr_t) {
    guts_.reset();
    return *this;
  }

  void clear() noexcept { *this = nullptr; }
  void swap(Result& other) noexcept { guts_.swap(other.guts_); }

  // Checks if the Result was successful.
  explicit operator bool() const { return !guts_; }
  bool ok() const { return !!*this; }
  void expect_ok(const char* file, unsigned int line) const;
  void ignore_ok() const;

  // Returns the Code for this Result.
  Code code() const noexcept {
    if (guts_) return guts_->code;
    return Code::OK;
  }

  // Returns the value of errno(3) associated with this Result.
  int errno_value() const {
    if (guts_) return guts_->err_no;
    return 0;
  }

  // Returns the message associated with this Result.
  const std::string& message() const {
    if (guts_) return guts_->message;
    static const std::string& empty = *new std::string;
    return empty;
  }

  // Helper for chaining together blocks of code, conditional on success.
  // Short-circuits to the first failure.
  //
  // Typical usage:
  //
  //    base::Result result = op1().and_then([] {
  //      return op2();    // only runs if op1() succeeded
  //    }).and_then([] {
  //      return op3();    // only runs if op1() and op2() both succeeded
  //    });
  //    CHECK_OK(result);  // throws if ANY of op1(), op2(), or op3() failed
  //
  template <typename F, typename... Args>
  base::Result and_then(F continuation, Args&&... args) const {
    if (guts_) return *this;
    return continuation(std::forward<Args>(args)...);
  }

  // Helper for chaining together blocks of code, conditional on failure.
  // Short-circuits to the first success.
  //
  // Typical usage:
  //
  //    base::Result result = op1().or_else([] {
  //      return op2();    // only runs if op1() failed
  //    }).or_else([] {
  //      return op3();    // only runs if op1() and op2() both failed
  //    });
  //    CHECK_OK(result);  // throws if ALL of op1(), op2(), and op3() failed
  //
  template <typename F, typename... Args>
  base::Result or_else(F continuation, Args&&... args) const {
    if (guts_) return continuation(std::forward<Args>(args)...);
    return *this;
  }

  // Stringifies this Result into a human-friendly form.
  void append_to(std::string& buffer) const;
  std::string as_string() const;

 private:
  std::shared_ptr<const Guts> guts_;
};

inline void swap(Result& a, Result& b) noexcept { a.swap(b); }
inline void append_to(std::string& buffer, Result::Code code) {
  buffer.append(Result::code_name(code));
}

// TODO: make these go away, subsumed by base/concat.h
inline std::ostream& operator<<(std::ostream& os, const Result& arg) {
  return (os << arg.as_string());
}
inline std::ostream& operator<<(std::ostream& os, Result::Code arg) {
  return (os << Result::code_name(arg));
}

}  // namespace base

#endif  // BASE_RESULT_H
