// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/result.h"

#include <cstring>
#include <exception>
#include <iostream>
#include <map>
#include <mutex>

#include "base/logging.h"

using Code = base::Result::Code;

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
      {EPERM, {"EPERM", Code::PERMISSION_DENIED}},
      {ENOENT, {"ENOENT", Code::NOT_FOUND}},
      {ESRCH, {"ESRCH", Code::NOT_FOUND}},
      {EINTR, {"EINTR", Code::ABORTED}},
      {EIO, {"EIO", Code::DATA_LOSS}},
      {ENXIO, {"ENXIO", Code::UNKNOWN}},
      {E2BIG, {"E2BIG", Code::INVALID_ARGUMENT}},
      {ENOEXEC, {"ENOEXEC", Code::FAILED_PRECONDITION}},
      {EBADF, {"EBADF", Code::INVALID_ARGUMENT}},
      {ECHILD, {"ECHILD", Code::NOT_FOUND}},
      {EAGAIN, {"EAGAIN", Code::ABORTED}},
      {ENOMEM, {"ENOMEM", Code::RESOURCE_EXHAUSTED}},
      {EACCES, {"EACCES", Code::PERMISSION_DENIED}},
      {EFAULT, {"EFAULT", Code::INVALID_ARGUMENT}},
      {ENOTBLK, {"ENOTBLK", Code::WRONG_TYPE}},
      {EBUSY, {"EBUSY", Code::UNAVAILABLE}},
      {EEXIST, {"EEXIST", Code::ALREADY_EXISTS}},
      {EXDEV, {"EXDEV", Code::INVALID_ARGUMENT}},
      {ENODEV, {"ENODEV", Code::NOT_FOUND}},
      {ENOTDIR, {"ENOTDIR", Code::WRONG_TYPE}},
      {EISDIR, {"EISDIR", Code::WRONG_TYPE}},
      {EINVAL, {"EINVAL", Code::INVALID_ARGUMENT}},
      {ENFILE, {"ENFILE", Code::RESOURCE_EXHAUSTED}},
      {EMFILE, {"EMFILE", Code::RESOURCE_EXHAUSTED}},
      {ENOTTY, {"ENOTTY", Code::FAILED_PRECONDITION}},
      {ETXTBSY, {"ETXTBSY", Code::UNAVAILABLE}},
      {EFBIG, {"EFBIG", Code::OUT_OF_RANGE}},
      {ENOSPC, {"ENOSPC", Code::RESOURCE_EXHAUSTED}},
      {ESPIPE, {"ESPIPE", Code::FAILED_PRECONDITION}},
      {EROFS, {"EROFS", Code::FAILED_PRECONDITION}},
      {EMLINK, {"EMLINK", Code::RESOURCE_EXHAUSTED}},
      {EPIPE, {"EPIPE", Code::CANCELLED}},
      {EDOM, {"EDOM", Code::OUT_OF_RANGE}},
      {ERANGE, {"ERANGE", Code::OUT_OF_RANGE}},
      {EDEADLK, {"EDEADLK", Code::FAILED_PRECONDITION}},
      {ENAMETOOLONG, {"ENAMETOOLONG", Code::INVALID_ARGUMENT}},
      {ENOLCK, {"ENOLCK", Code::RESOURCE_EXHAUSTED}},
      {ENOSYS, {"ENOSYS", Code::NOT_IMPLEMENTED}},
      {ENOTEMPTY, {"ENOTEMPTY", Code::FAILED_PRECONDITION}},
      {ELOOP, {"ELOOP", Code::FAILED_PRECONDITION}},
      {EWOULDBLOCK, {"EWOULDBLOCK", Code::ABORTED}},
      {EDEADLOCK, {"EDEADLOCK", Code::FAILED_PRECONDITION}},
      {ERESTART, {"ERESTART", Code::ABORTED}},
      {ENOTSOCK, {"ENOTSOCK", Code::WRONG_TYPE}},
      {EPROTONOSUPPORT, {"EPROTONOSUPPORT", Code::NOT_IMPLEMENTED}},
      {ESOCKTNOSUPPORT, {"ESOCKTNOSUPPORT", Code::NOT_IMPLEMENTED}},
      {EOPNOTSUPP, {"EOPNOTSUPP", Code::NOT_IMPLEMENTED}},
      {EPFNOSUPPORT, {"EPFNOSUPPORT", Code::NOT_IMPLEMENTED}},
      {EAFNOSUPPORT, {"EAFNOSUPPORT", Code::NOT_IMPLEMENTED}},
      {EADDRINUSE, {"EADDRINUSE", Code::UNAVAILABLE}},
      {EADDRNOTAVAIL, {"EADDRNOTAVAIL", Code::UNAVAILABLE}},
      {ENETDOWN, {"ENETDOWN", Code::UNAVAILABLE}},
      {ENETUNREACH, {"ENETUNREACH", Code::UNAVAILABLE}},
      {ENETRESET, {"ENETRESET", Code::UNAVAILABLE}},
      {ECONNABORTED, {"ECONNABORTED", Code::CANCELLED}},
      {ECONNRESET, {"ECONNRESET", Code::CANCELLED}},
      {ENOBUFS, {"ENOBUFS", Code::RESOURCE_EXHAUSTED}},
      {EISCONN, {"EISCONN", Code::FAILED_PRECONDITION}},
      {ENOTCONN, {"ENOTCONN", Code::FAILED_PRECONDITION}},
      {ESHUTDOWN, {"ESHUTDOWN", Code::FAILED_PRECONDITION}},
      {ETIMEDOUT, {"ETIMEDOUT", Code::DEADLINE_EXCEEDED}},
      {ECONNREFUSED, {"ECONNREFUSED", Code::UNAVAILABLE}},
      {EHOSTDOWN, {"EHOSTDOWN", Code::UNAVAILABLE}},
      {EHOSTUNREACH, {"EHOSTUNREACH", Code::UNAVAILABLE}},
      {EALREADY, {"EALREADY", Code::FAILED_PRECONDITION}},
      {EINPROGRESS, {"EINPROGRESS", Code::INTERNAL}},
      {ESTALE, {"ESTALE", Code::UNAVAILABLE}},
      {EDQUOT, {"EDQUOT", Code::RESOURCE_EXHAUSTED}},
      {ECANCELED, {"ECANCELED", Code::CANCELLED}},
  };
  return ref;
}

}  // anonymous namespace

namespace base {

std::mutex Result::s_mu;
const Result::NameMap* Result::s_name_map = nullptr;
const Result::MemoMap* Result::s_memo_map = nullptr;

Result::NameMap* Result::build_name_map() {
  std::unique_ptr<NameMap> map(new NameMap);
  auto f = [&map](Code x, std::string n) { (*map)[x] = n; };
  f(Code::OK, "OK");
#define MAP(x) f(Code::x, #x)
  MAP(UNKNOWN);
  MAP(INTERNAL);
  MAP(CANCELLED);
  MAP(FAILED_PRECONDITION);
  MAP(NOT_FOUND);
  MAP(ALREADY_EXISTS);
  MAP(WRONG_TYPE);
  MAP(PERMISSION_DENIED);
  MAP(UNAUTHENTICATED);
  MAP(INVALID_ARGUMENT);
  MAP(OUT_OF_RANGE);
  MAP(NOT_IMPLEMENTED);
  MAP(UNAVAILABLE);
  MAP(ABORTED);
  MAP(RESOURCE_EXHAUSTED);
  MAP(DEADLINE_EXCEEDED);
  MAP(DATA_LOSS);
  MAP(END_OF_FILE);
#undef MAP
  return map.release();
}

Result::MemoMap* Result::build_memo_map() {
  std::unique_ptr<MemoMap> map(new MemoMap);
  auto f = [&map](Code x) {
    (*map)[x] = std::make_shared<const Guts>(x, -1, std::string());
  };
#define MAP(x) f(Code::x)
  MAP(UNKNOWN);
  MAP(INTERNAL);
  MAP(CANCELLED);
  MAP(FAILED_PRECONDITION);
  MAP(NOT_FOUND);
  MAP(ALREADY_EXISTS);
  MAP(WRONG_TYPE);
  MAP(PERMISSION_DENIED);
  MAP(UNAUTHENTICATED);
  MAP(INVALID_ARGUMENT);
  MAP(OUT_OF_RANGE);
  MAP(NOT_IMPLEMENTED);
  MAP(UNAVAILABLE);
  MAP(ABORTED);
  MAP(RESOURCE_EXHAUSTED);
  MAP(DEADLINE_EXCEEDED);
  MAP(DATA_LOSS);
  MAP(END_OF_FILE);
#undef MAP
  return map.release();
}

const std::string& Result::code_name(Code code) noexcept {
  std::unique_lock<std::mutex> lock(s_mu);
  if (s_name_map == nullptr) s_name_map = build_name_map();
  auto it = s_name_map->find(code);
  if (it != s_name_map->end()) return it->second;
  static const auto& fallback = *new std::string("UNKNOWN");
  return fallback;
}

std::shared_ptr<const Result::Guts> Result::make(Code code, int err_no,
                                                 std::string&& message) {
  if (code == Code::OK) return nullptr;
  if (err_no == -1 && message.empty()) {
    std::unique_lock<std::mutex> lock(s_mu);
    if (s_memo_map == nullptr) s_memo_map = build_memo_map();
    auto it = s_memo_map->find(code);
    if (it != s_memo_map->end()) return it->second;
  }
  return std::make_shared<const Guts>(code, err_no, std::move(message));
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
  if (guts_) {
    Code code = guts_->code;
    int err_no = guts_->err_no;
    const auto& message = guts_->message;

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
  if (guts_) {
    Logger logger(file, line, 1, LOG_LEVEL_ERROR);
    logger << as_string();
  }
}

void Result::ignore_ok() const {}

}  // namespace base
