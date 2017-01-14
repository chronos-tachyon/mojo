#ifndef BASE_CPU_H
#define BASE_CPU_H

#include <vector>

#include "base/result.h"

namespace base {

struct CPUInfo {
  unsigned int node_id;
  unsigned int package_id;
  unsigned int core_id;
  unsigned int processor_id;

  CPUInfo(unsigned int n, unsigned int p, unsigned int c,
          unsigned int t) noexcept : node_id(n),
                                     package_id(p),
                                     core_id(c),
                                     processor_id(t) {}
  CPUInfo() noexcept : CPUInfo(0, 0, 0, 0) {}
};

inline bool operator==(CPUInfo a, CPUInfo b) noexcept {
  return a.node_id == b.node_id && a.package_id == b.package_id &&
         a.core_id == b.core_id && a.processor_id == b.processor_id;
}
inline bool operator!=(CPUInfo a, CPUInfo b) noexcept { return !(a == b); }
inline bool operator<(CPUInfo a, CPUInfo b) noexcept {
  if (a.node_id != b.node_id) return (a.node_id < b.node_id);
  if (a.package_id != b.package_id) return (a.package_id < b.package_id);
  if (a.core_id != b.core_id) return (a.core_id < b.core_id);
  return (a.processor_id < b.processor_id);
}
inline bool operator>(CPUInfo a, CPUInfo b) noexcept {
  return (b < a);
}
inline bool operator<=(CPUInfo a, CPUInfo b) noexcept {
  return !(b < a);
}
inline bool operator>=(CPUInfo a, CPUInfo b) noexcept {
  return !(a < b);
}

base::Result fetch_cpuinfo(std::vector<CPUInfo>* out);

const std::vector<CPUInfo>& cached_cpuinfo();

std::size_t num_nodes(const std::vector<CPUInfo>& vec);
std::size_t num_packages(const std::vector<CPUInfo>& vec);
std::size_t num_cores(const std::vector<CPUInfo>& vec);
std::size_t num_processors(const std::vector<CPUInfo>& vec);

}  // namespace base

#endif  // BASE_CPU_H
