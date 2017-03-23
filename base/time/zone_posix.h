// base/time/zone_posix.h - Low-level details of POSIX tzset(3) timezone specs
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_POSIX_H
#define BASE_TIME_POSIX_H

#include <string>

#include "base/result.h"
#include "base/strings.h"
#include "base/time/zone.h"

namespace base {
namespace time {
namespace zone {

struct PosixRules {
  std::string spec;
  Type standard_time;
  Type daylight_time;
  Recurrence dst_start;
  Recurrence dst_end;
};

Result parse_posix(PosixRules* out, StringPiece spec);
Pointer interpret_posix(const PosixRules& in);

}  // namespace zone
}  // namespace time
}  // namespace base

#endif  // BASE_TIME_POSIX_H
