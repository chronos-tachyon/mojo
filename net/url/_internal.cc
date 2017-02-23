#include "net/url/_internal.h"

static const char* const kDangerSet[] = {
    "", CC_DANGER_USERPASS, "", "", "", CC_DANGER_QUERYCOMP, "",
};

static const char* const kSafeSet[] = {
    CC_UNRESERVED, CC_AUTHORITY, CC_HOST,     CC_ZONE,
    CC_PATH,       CC_QUERY,     CC_FRAGMENT,
};

namespace net {
namespace url {
namespace internal {

bool is_safe(char ch, EscapeMode mode) noexcept {
  const char* danger_set = kDangerSet[static_cast<unsigned char>(mode)];
  const char* safe_set = kSafeSet[static_cast<unsigned char>(mode)];
  return !is_in(ch, danger_set) && is_in(ch, safe_set);
}

std::string escape(SP in, EscapeMode mode) {
  std::string out;
  out.reserve(in.size());
  for (char ch : in) {
    if (is_safe(ch, mode)) {
      out += ch;
    } else if (ch == ' ' && mode == EscapeMode::query_component) {
      out += '+';
    } else {
      out += '%';
      out += to_hex(static_cast<unsigned char>(ch) >> 4);
      out += to_hex(static_cast<unsigned char>(ch) & 15);
    }
  }
  return out;
}

std::pair<bool, std::string> unescape(SP in, EscapeMode mode) {
  std::string out;
  out.reserve(in.size());
  auto it = in.begin(), end = in.end();
  while (it != end) {
    if (*it == '+' && mode == EscapeMode::query_component) {
      out += ' ';
    } else if (*it == '%') {
      char hi, lo;
      ++it;
      if (it == end) return std::make_pair(false, "");
      hi = *it;
      if (!is_hex(hi)) return std::make_pair(false, "");
      ++it;
      if (it == end) return std::make_pair(false, "");
      lo = *it;
      if (!is_hex(lo)) return std::make_pair(false, "");
      out += static_cast<char>((from_hex(hi) << 4) | from_hex(lo));
    } else {
      out += *it;
    }
    ++it;
  }
  return std::make_pair(true, out);
}

}  // namespace internal
}  // namespace url
}  // namespace net
