// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <iostream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/time/zone.h"

int main(int argc, char** argv) {
  auto db = base::time::zone::system_database();
  std::vector<std::string> timezones;
  CHECK_OK(db->all(&timezones));
  for (const auto& timezone : timezones) {
    std::cout << timezone << "\n";
  }
  std::cout << std::flush;
  return 0;
}
