// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/result.h"

#include <cstring>
#include <exception>
#include <iostream>
#include <map>
#include <mutex>

#include "base/logging.h"

using Code = base::ResultCode;
using Rep = base::internal::ResultRep;
using RepPtr = std::shared_ptr<const Rep>;

namespace base {

namespace {

static std::string strerror_sane(int err_no) {
  char buf[256];
  ::bzero(buf, sizeof(buf));
  errno = 0;
  const char* ptr = ::strerror_r(err_no, buf, sizeof(buf));
  if (ptr) return ptr;
  return "<unknown errno code>";
}

struct Errno {
  const char* name;
  Code code;
};

static const std::map<int, Errno>& errno_map() {
  static const auto& ref = *new std::map<int, Errno>{
#define MAP(x, y) {x, {#x, Code::y}}
      MAP(EPERM, PERMISSION_DENIED),
      MAP(ENOENT, NOT_FOUND),
      MAP(ESRCH, NOT_FOUND),
      MAP(EINTR, ABORTED),
      MAP(EIO, DATA_LOSS),
      MAP(ENXIO, UNKNOWN),
      MAP(E2BIG, INVALID_ARGUMENT),
      MAP(ENOEXEC, FAILED_PRECONDITION),
      MAP(EBADF, INVALID_ARGUMENT),
      MAP(ECHILD, NOT_FOUND),
      MAP(EAGAIN, ABORTED),
      MAP(ENOMEM, RESOURCE_EXHAUSTED),
      MAP(EACCES, PERMISSION_DENIED),
      MAP(EFAULT, INVALID_ARGUMENT),
      MAP(ENOTBLK, WRONG_TYPE),
      MAP(EBUSY, UNAVAILABLE),
      MAP(EEXIST, ALREADY_EXISTS),
      MAP(EXDEV, INVALID_ARGUMENT),
      MAP(ENODEV, NOT_FOUND),
      MAP(ENOTDIR, WRONG_TYPE),
      MAP(EISDIR, WRONG_TYPE),
      MAP(EINVAL, INVALID_ARGUMENT),
      MAP(ENFILE, RESOURCE_EXHAUSTED),
      MAP(EMFILE, RESOURCE_EXHAUSTED),
      MAP(ENOTTY, FAILED_PRECONDITION),
      MAP(ETXTBSY, UNAVAILABLE),
      MAP(EFBIG, OUT_OF_RANGE),
      MAP(ENOSPC, RESOURCE_EXHAUSTED),
      MAP(ESPIPE, FAILED_PRECONDITION),
      MAP(EROFS, FAILED_PRECONDITION),
      MAP(EMLINK, RESOURCE_EXHAUSTED),
      MAP(EPIPE, CANCELLED),
      MAP(EDOM, OUT_OF_RANGE),
      MAP(ERANGE, OUT_OF_RANGE),
      MAP(EDEADLK, FAILED_PRECONDITION),
      MAP(ENAMETOOLONG, INVALID_ARGUMENT),
      MAP(ENOLCK, RESOURCE_EXHAUSTED),
      MAP(ENOSYS, NOT_IMPLEMENTED),
      MAP(ENOTEMPTY, FAILED_PRECONDITION),
      MAP(ELOOP, FAILED_PRECONDITION),
      MAP(EWOULDBLOCK, ABORTED),
      MAP(EDEADLOCK, FAILED_PRECONDITION),
      MAP(ERESTART, ABORTED),
      MAP(ENOTSOCK, WRONG_TYPE),
      MAP(EPROTONOSUPPORT, NOT_IMPLEMENTED),
      MAP(ESOCKTNOSUPPORT, NOT_IMPLEMENTED),
      MAP(EOPNOTSUPP, NOT_IMPLEMENTED),
      MAP(EPFNOSUPPORT, NOT_IMPLEMENTED),
      MAP(EAFNOSUPPORT, NOT_IMPLEMENTED),
      MAP(EADDRINUSE, UNAVAILABLE),
      MAP(EADDRNOTAVAIL, UNAVAILABLE),
      MAP(ENETDOWN, UNAVAILABLE),
      MAP(ENETUNREACH, UNAVAILABLE),
      MAP(ENETRESET, UNAVAILABLE),
      MAP(ECONNABORTED, CANCELLED),
      MAP(ECONNRESET, CANCELLED),
      MAP(ENOBUFS, RESOURCE_EXHAUSTED),
      MAP(EISCONN, FAILED_PRECONDITION),
      MAP(ENOTCONN, FAILED_PRECONDITION),
      MAP(ESHUTDOWN, FAILED_PRECONDITION),
      MAP(ETIMEDOUT, DEADLINE_EXCEEDED),
      MAP(ECONNREFUSED, UNAVAILABLE),
      MAP(EHOSTDOWN, UNAVAILABLE),
      MAP(EHOSTUNREACH, UNAVAILABLE),
      MAP(EALREADY, FAILED_PRECONDITION),
      MAP(EINPROGRESS, INTERNAL),
      MAP(ESTALE, UNAVAILABLE),
      MAP(EDQUOT, RESOURCE_EXHAUSTED),
      MAP(ECANCELED, CANCELLED),
#undef MAP
  };
  return ref;
}

static const std::map<Code, std::string>& name_map() {
  static const auto& ref = *new std::map<Code, std::string>{
#define MAP(x) {Code::x, #x}
      MAP(OK),
      MAP(UNKNOWN),
      MAP(INTERNAL),
      MAP(CANCELLED),
      MAP(FAILED_PRECONDITION),
      MAP(NOT_FOUND),
      MAP(ALREADY_EXISTS),
      MAP(WRONG_TYPE),
      MAP(PERMISSION_DENIED),
      MAP(UNAUTHENTICATED),
      MAP(INVALID_ARGUMENT),
      MAP(OUT_OF_RANGE),
      MAP(NOT_IMPLEMENTED),
      MAP(UNAVAILABLE),
      MAP(ABORTED),
      MAP(RESOURCE_EXHAUSTED),
      MAP(DEADLINE_EXCEEDED),
      MAP(DATA_LOSS),
      MAP(END_OF_FILE),
#undef MAP
  };
  return ref;
}

static const std::map<Code, RepPtr>& memo_map() {
  static const auto& ref = *new std::map<Code, RepPtr>{
#define MAP(x) {Code::x, \
                std::make_shared<const Rep>(Code::x, -1, std::string())}
      MAP(UNKNOWN),
      MAP(INTERNAL),
      MAP(CANCELLED),
      MAP(FAILED_PRECONDITION),
      MAP(NOT_FOUND),
      MAP(ALREADY_EXISTS),
      MAP(WRONG_TYPE),
      MAP(PERMISSION_DENIED),
      MAP(UNAUTHENTICATED),
      MAP(INVALID_ARGUMENT),
      MAP(OUT_OF_RANGE),
      MAP(NOT_IMPLEMENTED),
      MAP(UNAVAILABLE),
      MAP(ABORTED),
      MAP(RESOURCE_EXHAUSTED),
      MAP(DEADLINE_EXCEEDED),
      MAP(DATA_LOSS),
      MAP(END_OF_FILE),
#undef MAP
  };
  return ref;
}

}  // anonymous namespace

namespace internal {
const std::string& empty_string() noexcept {
  static const auto& ref = *new std::string;
  return ref;
}
}  // namespace internal

const std::string& resultcode_name(Code code) noexcept {
  const auto& map = name_map();
  auto it = map.find(code);
  if (it != map.end()) return it->second;
  return internal::empty_string();
}

RepPtr Result::make(Code code, int err_no, std::string message) {
  if (code == Code::OK) return nullptr;
  if (err_no == -1 && message.empty()) {
    const auto& map = memo_map();
    auto it = map.find(code);
    if (it != map.end()) return it->second;
  }
  return std::make_shared<const Rep>(code, err_no, std::move(message));
}

Result Result::from_errno(int err_no, std::string what) {
  if (err_no == 0) return Result();
  Code code = Code::UNKNOWN;
  const auto& map = errno_map();
  auto it = map.find(err_no);
  if (it != map.end()) code = it->second.code;
  return Result(code, std::move(what), err_no);
}

void Result::append_to(std::string* out) const {
  if (rep_) {
    Code code = rep_->code;
    int err_no = rep_->err_no;
    const auto& message = rep_->message;

    concat_to(out, code_name(code), '(', static_cast<uint16_t>(code), ')');
    if (!message.empty()) {
      concat_to(out, ": ", message);
    }
    if (err_no != 0 && err_no != -1) {
      const auto& map = errno_map();
      auto it = map.find(err_no);
      if (it == map.end())
        concat_to(out, " errno:[#", err_no);
      else
        concat_to(out, " errno:[", it->second.name);
      auto errstr = strerror_sane(err_no);
      if (errstr.empty())
        concat_to(out, ']');
      else
        concat_to(out, ' ', errstr, ']');
    }
  } else {
    concat_to(out, "OK(0)");
  }
}

std::string Result::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

void Result::expect_ok(const char* file, unsigned int line) const {
  if (rep_) {
    Logger logger(file, line, 1, LOG_LEVEL_ERROR);
    logger << as_string();
  }
}

void Result::ignore_ok() const {}

}  // namespace base
