// base/time/zone_tzif.h - Low-level details of TZif zoneinfo timezone files
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_TZIF_H
#define BASE_TIME_TZIF_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/result.h"
#include "base/strings.h"
#include "base/time/time.h"
#include "base/time/zone.h"
#include "base/time/zone_posix.h"

namespace base {
namespace time {
namespace zone {

struct TZifFile {
  std::string filename;
  std::vector<Type> types;
  std::vector<Time> times;
  std::vector<uint8_t> indices;
  std::vector<LeapSecond> leaps;
  std::vector<bool> ttisstd;
  std::vector<bool> ttisgmt;
  std::unique_ptr<PosixRules> posix;
  char version;

  TZifFile() : version(0) {}
};

Result parse_tzif(TZifFile* out, StringPiece filename, StringPiece data);
Pointer interpret_tzif(const TZifFile& in);

}  // namespace zone
}  // namespace time
}  // namespace base

#endif  // BASE_TIME_TZIF_H
