#ifndef NET_URL_INTERNAL_H
#define NET_URL_INTERNAL_H

#include <string>
#include <tuple>

#include "base/strings.h"

namespace net {
namespace url {
namespace internal {

/*
 * https://url.spec.whatwg.org/
 * https://www.ietf.org/rfc/rfc3986.txt
 * https://en.wikipedia.org/wiki/Uniform_Resource_Identifier
 */

#define CC_DIGIT "0123456789"
#define CC_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define CC_LOWER "abcdefghijklmnopqrstuvwxyz"
#define CC_ALPHA CC_UPPER CC_LOWER
#define CC_GEN_DELIMS "#/:?@[]"
#define CC_SUB_DELIMS "!$&'()*+,;="
#define CC_RESERVED CC_GEN_DELIMS CC_SUB_DELIMS
#define CC_UNRESERVED CC_DIGIT CC_ALPHA "-._~"
#define CC_PCHAR CC_UNRESERVED CC_SUB_DELIMS ":@"
#define CC_SCHEME CC_DIGIT CC_ALPHA "+-."
#define CC_HOST CC_UNRESERVED CC_SUB_DELIMS ":[]"
#define CC_ZONE CC_HOST "\"<>"
#define CC_AUTHORITY CC_PCHAR
#define CC_PATH CC_PCHAR "/"
#define CC_QUERY CC_PCHAR "/?"
#define CC_FRAGMENT CC_PCHAR "/?"

#define CC_DANGER_USERPASS ":@"
#define CC_DANGER_QUERYCOMP "?&;="

using SP = base::StringPiece;

enum class EscapeMode : unsigned char {
  userinfo = 1,
  hostname = 2,
  ipv6zone = 3,
  path = 4,
  query_component = 5,
  fragment = 6,
};

inline void to_lower(std::string& str) {
  for (char& ch : str) {
    if (ch >= 'A' && ch <= 'Z') {
      ch += ('a' - 'A');
    }
  }
}

inline bool is_hex(char ch) noexcept {
  return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') ||
         (ch >= 'a' && ch <= 'f');
}

inline char to_hex(unsigned char value) noexcept {
  return "0123456789ABCDEF"[value & 0xf];
}

inline unsigned char from_hex(char ch) noexcept {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  } else if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  } else {
    return 0;
  }
}

inline bool is_in(char ch, const char* set) noexcept {
  while (*set) {
    if (ch == *set) return true;
    ++set;
  }
  return false;
}

inline std::tuple<SP, SP, bool> split(SP in, char ch) {
  auto i = in.find(ch);
  if (i == SP::npos) {
    return std::make_tuple(in, nullptr, false);
  }
  return std::make_tuple(in.substring(0, i), in.substring(i + 1), true);
}

inline std::tuple<SP, SP, bool> split_scheme(SP in) {
  for (std::size_t i = 0; i < in.size(); i++) {
    char ch = in[i];
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      // pass
    } else if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' ||
               ch == '.') {
      if (i == 0) break;
    } else if (ch == ':') {
      return std::make_tuple(in.substring(0, i), in.substring(i + 1), true);
    } else {
      break;
    }
  }
  return std::make_tuple(nullptr, in, false);
}

bool is_safe(char ch, EscapeMode mode) noexcept;
std::string escape(SP in, EscapeMode mode);
std::pair<bool, std::string> unescape(SP in, EscapeMode mode);

}  // namespace internal
}  // namespace url
}  // namespace net

#endif  // NET_URL_INTERNAL_H
