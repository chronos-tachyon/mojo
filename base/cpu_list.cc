// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <cstdio>
#include <vector>

#include "base/cpu.h"

int main(int argc, char** argv) {
  const auto& cpus = base::cached_cpuinfo();
  for (const auto& cpu : cpus) {
    printf("[%u, %u, %u, %u]\n", cpu.node_id, cpu.package_id, cpu.core_id,
           cpu.processor_id);
  }
  return 0;
}
