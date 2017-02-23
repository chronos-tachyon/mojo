#include "net/url/url.h"

#include <algorithm>
#include <tuple>

#include "base/logging.h"
#include "net/url/_internal.h"

namespace net {
namespace url {

using internal::EscapeMode;

base::Result URL::parse(base::StringPiece raw, bool via_request) {
  clear();

  if (raw.empty()) {
    return base::Result::invalid_argument("empty URL");
  }

  if (via_request && raw == "*") {
    CHECK_OK(set_raw_path(raw));
    return base::Result();
  }

  bool has_scheme;
  base::StringPiece scheme;
  std::tie(scheme, raw, has_scheme) = internal::split_scheme(raw);
  if (has_scheme) {
    if (scheme.empty())
      return base::Result::invalid_argument("missing URL scheme");
    set_scheme(scheme);
  }

  if (!via_request) {
    bool has_fragment;
    std::string fragment;
    std::tie(raw, fragment, has_fragment) = internal::split(raw, '#');
    if (has_fragment) {
      auto result = set_raw_fragment(fragment);
      if (!result) return result;
    }
  }

  bool has_query;
  base::StringPiece query;
  std::tie(raw, query, has_query) = internal::split(raw, '?');
  if (has_query) set_raw_query(query);

  if (raw.empty() || raw.front() != '/') {
    if (!has_scheme)
      return base::Result::invalid_argument("invalid URI for request");
    set_opaque(raw);
    return base::Result();
  }

  bool has_path = !raw.empty();
  std::string raw_path;
  if ((!via_request || has_scheme) && has_prefix(raw, "//")) {
    base::StringPiece authority, path;
    std::tie(authority, path, has_path) =
        internal::split(raw.substring(2), '/');
    if (has_path) {
      raw_path.reserve(1 + path.size());
      raw_path.push_back('/');
      path.append_to(&raw_path);
    }

    bool has_userinfo;
    std::string userinfo, hostname;
    std::tie(userinfo, hostname, has_userinfo) =
        internal::split(authority, '@');

    if (has_userinfo) {
      bool has_password;
      std::string username, password;
      std::tie(username, password, has_password) =
          internal::split(userinfo, ':');

      auto username_result = internal::unescape(username, EscapeMode::userinfo);
      auto password_result = internal::unescape(password, EscapeMode::userinfo);
      if (!username_result.first)
        return base::Result::invalid_argument(
            "malformed '%'-escape in username");
      if (!password_result.first)
        return base::Result::invalid_argument(
            "malformed '%'-escape in password");

      if (has_password)
        set_userinfo(std::move(username_result.second),
                     std::move(password_result.second));
      else
        set_userinfo(std::move(username_result.second));
    } else {
      userinfo.swap(hostname);
    }

    auto hostname_result = internal::unescape(hostname, EscapeMode::hostname);
    if (!hostname_result.first)
      return base::Result::invalid_argument("malformed '%'-escape in hostname");
    if (!hostname_result.second.empty())
      set_hostname(std::move(hostname_result.second));
  }

  if (has_path) {
    auto result = set_raw_path(raw_path);
    if (!result) return result;
  }

  return base::Result();
}

void URL::clear() {
  has_ = 0;
  scheme_.clear();
  opaque_.clear();
  username_.clear();
  password_.clear();
  hostname_.clear();
  path_.clear();
  raw_path_.clear();
  raw_query_.clear();
  fragment_.clear();
  raw_fragment_.clear();
  query_.clear();
}

void URL::swap(URL& other) noexcept {
  using std::swap;
  swap(has_, other.has_);
  swap(scheme_, other.scheme_);
  swap(opaque_, other.opaque_);
  swap(username_, other.username_);
  swap(password_, other.password_);
  swap(hostname_, other.hostname_);
  swap(path_, other.path_);
  swap(raw_path_, other.raw_path_);
  swap(raw_query_, other.raw_query_);
  swap(fragment_, other.fragment_);
  swap(raw_fragment_, other.raw_fragment_);
  swap(query_, other.query_);
}

void URL::normalize() {
  internal::to_lower(scheme_);
  internal::to_lower(hostname_);
  if (has_path()) {
    raw_path_ = internal::escape(path_, EscapeMode::path);
  } else if (has_hostname()) {
    has_ |= bit_path;
    path_ = "/";
    raw_path_ = "/";
  }
  if (has_query()) {
    raw_query_ = query_.as_string();
    if (raw_query_.empty()) clear_query();
  }
  if (has_fragment()) {
    raw_fragment_ = internal::escape(fragment_, EscapeMode::fragment);
    if (raw_fragment_.empty()) clear_fragment();
  }
}

void URL::clear_scheme() {
  has_ &= ~bit_scheme;
  scheme_.clear();
}

void URL::set_scheme(base::StringPiece scheme) {
  has_ |= bit_scheme;
  scheme_ = std::move(scheme);
}

void URL::clear_opaque() {
  has_ &= ~bit_opaque;
  opaque_.clear();
}

void URL::set_opaque(base::StringPiece opaque) {
  has_ |= bit_opaque;
  opaque_ = std::move(opaque);
}

void URL::clear_userinfo() {
  has_ &= ~(bit_username | bit_password);
  username_.clear();
  password_.clear();
}

void URL::set_userinfo(base::StringPiece username) {
  has_ = (has_ | bit_username) & ~bit_password;
  username_ = std::move(username);
  password_.clear();
}

void URL::set_userinfo(base::StringPiece username, base::StringPiece password) {
  has_ |= bit_username | bit_password;
  username_ = std::move(username);
  password_ = std::move(password);
}

void URL::clear_hostname() {
  has_ &= ~bit_hostname;
  hostname_.clear();
}

void URL::set_hostname(base::StringPiece hostname) {
  has_ |= bit_hostname;
  hostname_ = std::move(hostname);
}

void URL::clear_path() {
  has_ &= ~bit_path;
  path_.clear();
  raw_path_.clear();
}

base::Result URL::set_path(base::StringPiece path) {
  if (path.empty()) {
    path = "/";
  }

  if (path == "*") {
    has_ |= bit_path;
    path_ = path;
    raw_path_ = std::move(path);
    return base::Result();
  }

  if (path.front() != '/')
    return base::Result::invalid_argument("path must start with '/'");

  auto raw = internal::escape(path, EscapeMode::path);
  has_ |= bit_path;
  path_ = std::move(path);
  raw_path_ = std::move(raw);
  return base::Result();
}

base::Result URL::set_raw_path(base::StringPiece path) {
  if (path.empty()) {
    path = "/";
  }

  if (path == "*") {
    has_ |= bit_path;
    path_ = path;
    raw_path_ = std::move(path);
    return base::Result();
  }

  if (path.front() != '/')
    return base::Result::invalid_argument("path must start with '/'");

  auto result = internal::unescape(path, EscapeMode::path);
  if (!result.first)
    return base::Result::invalid_argument("invalid escaped path");

  has_ |= bit_path;
  path_ = std::move(result.second);
  raw_path_ = std::move(path);
  return base::Result();
}

void URL::clear_query() {
  has_ &= ~bit_query;
  query_.clear();
  raw_query_.clear();
}

base::Result URL::set_raw_query(base::StringPiece query) {
  Query tmp;
  auto result = tmp.parse(query);
  if (!result) return result;
  has_ |= bit_query;
  query_.swap(tmp);
  raw_query_ = query;
  return base::Result();
}

void URL::clear_fragment() {
  has_ &= ~bit_fragment;
  fragment_.clear();
  raw_fragment_.clear();
}

void URL::set_fragment(base::StringPiece fragment) {
  has_ |= bit_fragment;
  fragment_ = fragment;
  raw_fragment_ = internal::escape(fragment, EscapeMode::fragment);
}

base::Result URL::set_raw_fragment(base::StringPiece fragment) {
  auto result = internal::unescape(fragment, EscapeMode::fragment);
  if (!result.first)
    return base::Result::invalid_argument("malformed '%'-escape in fragment");
  has_ |= bit_fragment;
  fragment_ = std::move(result.second);
  raw_fragment_ = std::move(fragment);
  return base::Result();
}

void URL::append_to(std::string* out) const {
  if (has_scheme()) {
    out->append(scheme_);
    out->push_back(':');
  }

  if (has_opaque()) {
    out->append(opaque_);
  } else {
    if (has_scheme() || has_username() || has_hostname()) {
      out->append("//");
      if (has_username()) {
        out->append(internal::escape(username_, EscapeMode::userinfo));
        if (has_password()) {
          out->push_back(':');
          out->append(internal::escape(password_, EscapeMode::userinfo));
        }
        out->push_back('@');
      }
      if (has_hostname()) {
        out->append(internal::escape(hostname_, EscapeMode::hostname));
      }
    }
    if (has_path()) {
      out->append(raw_path_);
    }
  }

  if (has_query()) {
    out->push_back('?');
    out->append(raw_query_);
  }

  if (has_fragment()) {
    out->push_back('#');
    out->append(raw_fragment_);
  }
}

std::string URL::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

bool operator==(const URL& a, const URL& b) {
  if (a.has_ != b.has_) return false;
  if (a.scheme_ != b.scheme_) return false;
  if (a.opaque_ != b.opaque_) return false;
  if (a.username_ != b.username_) return false;
  if (a.password_ != b.password_) return false;
  if (a.hostname_ != b.hostname_) return false;
  if (a.raw_path_ != b.raw_path_) return false;
  if (a.raw_query_ != b.raw_query_) return false;
  if (a.raw_fragment_ != b.raw_fragment_) return false;
  return true;
}

bool URL::equivalent_to(const URL& other) const {
  URL a = *this, b = other;
  a.normalize();
  b.normalize();
  return (a == b);
}

}  // namespace url
}  // namespace net
