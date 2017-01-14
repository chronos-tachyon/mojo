// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/cpu.h"

#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>

#include "base/concat.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/strings.h"
#include "external/com_googlesource_code_re2/re2/re2.h"

namespace base {

static base::Result parse_uint(unsigned int* out, std::string str) {
  CHECK_NOTNULL(out);
  *out = 0;

  const char* ptr = str.c_str();
  const char* end = ptr;
  errno = 0;
  unsigned long value = ::strtoul(ptr, const_cast<char**>(&end), 10);
  int err_no = errno;
  if (err_no == 0 && value > UINT_MAX) err_no = ERANGE;
  if (err_no != 0) return base::Result::from_errno(err_no, "strtoul(3)");
  *out = value;
  return base::Result();
}

static base::Result parse_list(std::vector<unsigned int>* out, StringPiece sp) {
  CHECK_NOTNULL(out);
  out->clear();

  auto comma = split::on(',').trim_whitespace();
  auto dash = split::on('-').limit(2);

  base::Result r;
  unsigned int value0, value1;
  for (StringPiece piece : comma.split(sp)) {
    auto numbers = dash.split(piece);
    r = parse_uint(&value0, numbers[0]);
    if (numbers.size() > 1) {
      r = r.and_then(parse_uint(&value1, numbers[1]));
    } else {
      value1 = value0;
    }
    if (!r) return r;
    out->push_back(value0);
    while (value0 < value1) {
      ++value0;
      out->push_back(value0);
    }
  }
  return base::Result();
}

static base::Result listdir(std::vector<DEntry>* out, const char* path) {
  int fdnum = ::open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fdnum == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "open(2) path=", path);
  }
  auto fd = wrapfd(fdnum);
  auto r = readdir_all(out, fd, path);
  r = r.and_then(fd->close());
  return r;
}

static base::Result readfile(std::vector<char>* out, const char* path) {
  int fdnum = ::open(path, O_RDONLY | O_CLOEXEC);
  if (fdnum == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "open(2) path=", path);
  }
  auto fd = wrapfd(fdnum);
  auto r = read_all(out, fd, path);
  r = r.and_then(fd->close());
  return r;
}

base::Result fetch_cpuinfo(std::vector<CPUInfo>* out) {
  std::vector<CPUInfo> tmp;
  std::vector<DEntry> dents;
  std::vector<char> buf;
  std::string path;

  CHECK_NOTNULL(out);
  out->clear();

  auto r = listdir(&dents, "/sys/devices/system/node");
  if (!r) return r;

  for (const auto& dent : dents) {
    std::string name = std::get<2>(dent);
    StringPiece sp(name);
    if (!sp.remove_prefix("node")) continue;

    unsigned int node_id;
    r = parse_uint(&node_id, sp);
    if (!r) continue;

    path = concat("/sys/devices/system/node/", name, "/cpulist");
    r = readfile(&buf, path.c_str());
    if (!r) return r;

    std::vector<unsigned int> processor_ids;
    r = parse_list(&processor_ids, buf);
    if (!r) return r;

    for (unsigned int id : processor_ids) {
      path = concat("/sys/devices/system/cpu/cpu", id, "/topology/core_id");
      r = readfile(&buf, path.c_str());
      if (!r) return r;

      sp = buf;
      sp.trim_whitespace();
      unsigned int core_id;
      r = parse_uint(&core_id, sp);
      if (!r) return r;

      path = concat("/sys/devices/system/cpu/cpu", id,
                    "/topology/physical_package_id");
      r = readfile(&buf, path.c_str());
      if (!r) return r;

      sp = buf;
      sp.trim_whitespace();
      unsigned int package_id;
      r = parse_uint(&package_id, sp);
      if (!r) return r;

      tmp.emplace_back(node_id, package_id, core_id, id);
    }
  }
  std::sort(tmp.begin(), tmp.end());
  *out = tmp;
  return base::Result();
}

static std::vector<CPUInfo> must_fetch_cpuinfo() {
  std::vector<CPUInfo> out;
  CHECK_OK(fetch_cpuinfo(&out));
  LOG(INFO) << "/proc/cpuinfo"
            << ": " << num_nodes(out) << " nodes"
            << ", " << num_packages(out) << " packages"
            << ", " << num_cores(out) << " cores"
            << ", " << num_processors(out) << " hyperthreads";
  return out;
}

const std::vector<CPUInfo>& cached_cpuinfo() {
  static std::vector<CPUInfo>& ref =
      *new std::vector<CPUInfo>(must_fetch_cpuinfo());
  return ref;
}

template <typename Func>
static std::size_t num(const std::vector<CPUInfo>& vec, Func func) {
  std::set<unsigned int> set;
  for (const auto& cpu : vec) {
    set.insert(func(cpu));
  }
  return set.size();
}

std::size_t num_nodes(const std::vector<CPUInfo>& vec) {
  return num(vec, [](const CPUInfo& cpu) { return cpu.node_id; });
}

std::size_t num_packages(const std::vector<CPUInfo>& vec) {
  return num(vec, [](const CPUInfo& cpu) { return cpu.package_id; });
}

std::size_t num_cores(const std::vector<CPUInfo>& vec) {
  return num(vec, [](const CPUInfo& cpu) { return cpu.core_id; });
}

std::size_t num_processors(const std::vector<CPUInfo>& vec) {
  return num(vec, [](const CPUInfo& cpu) { return cpu.processor_id; });
}

using Map = std::map<unsigned int, std::vector<CPUInfo>>;
using Vec = std::vector<unsigned int>;

static std::mutex g_mu;
static Map* g_map = nullptr;
static Vec* g_vec = nullptr;
static std::size_t g_next = 0;

static void next_core(std::vector<CPUInfo>* out) {
  CHECK_NOTNULL(out);
  out->clear();

  const auto& cpus = cached_cpuinfo();

  auto lock = acquire_lock(g_mu);

  if (!g_vec) {
    std::unique_ptr<Map> map(new Map);
    std::unique_ptr<Vec> vec(new Vec);
    for (const auto& cpu : cpus) {
      auto& core = (*map)[cpu.core_id];
      if (core.empty()) vec->push_back(cpu.core_id);
      core.push_back(cpu);
    }
    g_map = map.release();
    g_vec = vec.release();
  }

  std::size_t next = g_next;
  g_next = (g_next + 1) % g_vec->size();

  unsigned int core_id = (*g_vec)[next];
  const auto& core = (*g_map)[core_id];
  out->insert(out->end(), core.begin(), core.end());
}

static pid_t my_gettid() { return syscall(SYS_gettid); }

Result allocate_core() {
  std::vector<CPUInfo> cpus;
  next_core(&cpus);

  std::string str;
  cpu_set_t set;
  CPU_ZERO(&set);
  for (const auto& cpu : cpus) {
    concat_to(&str, cpu.processor_id, ", ");
    CPU_SET(cpu.processor_id, &set);
  }
  if (!str.empty()) str.resize(str.size() - 2);

  int rc = sched_setaffinity(0, sizeof(set), &set);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "sched_setaffinity(2)");
  }
  VLOG(1) << "Pinned thread " << my_gettid() << " to CPUs: " << str;
  return base::Result();
}

}  // namespace base
