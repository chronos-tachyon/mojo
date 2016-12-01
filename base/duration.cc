// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/duration.h"

#include <sstream>

namespace base {

void Duration::append_to(std::string& out) const {
  std::ostringstream os;
  os << std::boolalpha << "Duration(" << neg_ << ", " << s_ << ", " << ns_ << ")";
  out.append(os.str());
}

std::string Duration::as_string() const {
  std::string out;
  append_to(out);
  return out;
}

}  // namespace base
