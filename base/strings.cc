#include "base/strings.h"

namespace base {

constexpr std::size_t StringPiece::npos;

void StringPiece::append_to(std::string* out) const {
  out->append(data_, size_);
}

std::string StringPiece::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

std::ostream& operator<<(std::ostream& o, StringPiece sp) {
  return (o << sp.as_string());
}

}  // namespace base
