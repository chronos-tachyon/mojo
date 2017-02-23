#ifndef NET_URL_URL_H
#define NET_URL_URL_H

#include <string>

#include "base/result.h"
#include "base/strings.h"
#include "net/url/query.h"

namespace net {
namespace url {

class URL {
 private:
  enum bits {
    bit_scheme = (1U << 0),
    bit_opaque = (1U << 1),
    bit_username = (1U << 2),
    bit_password = (1U << 3),
    bit_hostname = (1U << 4),
    bit_path = (1U << 5),
    bit_query = (1U << 6),
    bit_fragment = (1U << 7),
  };

  bool has(uint8_t bits) const noexcept { return (has_ & bits) != 0; }

 public:
  URL() : has_(0) {}

  base::Result parse(base::StringPiece raw, bool via_request = false);
  void normalize();

  explicit operator bool() const { return !empty(); }
  bool empty() const { return has_ == 0; }

  void clear();
  void swap(URL& other) noexcept;

  void clear_scheme();
  void set_scheme(base::StringPiece scheme);
  bool has_scheme() const { return has(bit_scheme); }
  base::StringPiece scheme() const { return scheme_; }

  void clear_opaque();
  void set_opaque(base::StringPiece opaque);
  bool has_opaque() const { return has(bit_opaque); }
  base::StringPiece opaque() const { return opaque_; }

  void clear_userinfo();
  void set_userinfo(base::StringPiece username);
  void set_userinfo(base::StringPiece username, base::StringPiece password);
  bool has_username() const { return has(bit_username); }
  bool has_password() const { return has(bit_password); }
  base::StringPiece username() const { return username_; }
  base::StringPiece password() const { return password_; }

  void clear_hostname();
  void set_hostname(base::StringPiece hostname);
  bool has_hostname() const { return has(bit_hostname); }
  base::StringPiece hostname() const { return hostname_; }

  void clear_path();
  base::Result set_path(base::StringPiece path);
  base::Result set_raw_path(base::StringPiece path);
  bool has_path() const { return has(bit_path); }
  base::StringPiece path() const { return path_; }
  base::StringPiece raw_path() const { return raw_path_; }

  void clear_query();
  base::Result set_raw_query(base::StringPiece query);
  bool has_query() const { return has(bit_query); }
  base::StringPiece raw_query() const { return raw_query_; }

  void clear_fragment();
  void set_fragment(base::StringPiece fragment);
  base::Result set_raw_fragment(base::StringPiece fragment);
  bool has_fragment() const { return has(bit_fragment); }
  base::StringPiece fragment() const { return fragment_; }
  base::StringPiece raw_fragment() const { return raw_fragment_; }

  void append_to(std::string* out) const;
  std::string as_string() const;
  operator std::string() const { return as_string(); }

  friend bool operator==(const URL& a, const URL& b);
  friend bool operator!=(const URL& a, const URL& b) { return !(a == b); }
  bool equivalent_to(const URL& other) const;

 private:
  uint8_t has_;
  std::string scheme_;
  std::string opaque_;
  std::string username_;
  std::string password_;
  std::string hostname_;
  std::string path_;
  std::string raw_path_;
  std::string raw_query_;
  std::string fragment_;
  std::string raw_fragment_;
  Query query_;
};

inline void swap(URL& a, URL& b) noexcept { a.swap(b); }

}  // namespace url
}  // namespace net

#endif  // NET_URL_URL_H
