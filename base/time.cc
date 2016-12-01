// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time.h"

#include <cstring>
#include <stdexcept>
#include <time.h>
#include <sstream>

namespace base {

void Time::append_to(std::string& out) const {
  std::ostringstream os;
  os << "Time(" << d_.as_string() << ")";
  out.append(os.str());
}

std::string Time::as_string() const {
  std::string out;
  append_to(out);
  return out;
}

}  // namespace base
