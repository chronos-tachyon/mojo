// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/zone.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <limits>
#include <mutex>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "base/concat.h"
#include "base/endian.h"
#include "base/env.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/strings.h"
#include "base/time/breakdown.h"
#include "base/time/duration.h"
#include "base/time/zone_posix.h"
#include "base/time/zone_tzif.h"
#include "external/com_googlesource_code_re2/re2/re2.h"

static std::mutex g_mu;
static base::time::zone::DatabasePointer* g_sysdb = nullptr;

static std::vector<std::string> list_zoneinfo_dirs() {
  const char* envvar = base::env::safe_get("TZDIR");
  if (envvar) {
    std::vector<std::string> out;
    out.push_back(envvar);
    return out;
  }

  std::vector<std::string> out;
  for (const auto& dir : base::env::xdg_data_dirs("zoneinfo")) {
    out.push_back(dir);
  }

  // Fallback candidates
  std::vector<std::string> candidates;
  candidates.push_back("/usr/share/lib/zoneinfo");  // Solaris, apparently
  candidates.push_back("/usr/lib/locale/TZ");       // IRIX, apparently
  candidates.push_back("/usr/local/etc/zoneinfo");  // tzcode default
  for (const auto& dir : candidates) {
    struct stat st;
    ::bzero(&st, sizeof(st));
    int rc = ::stat(dir.c_str(), &st);
    if (rc != 0) {
      int err_no = errno;
      if (err_no == ENOENT) continue;
    }
    if (!S_ISDIR(st.st_mode)) continue;
    out.push_back(dir);
  }

  return out;
}

static void sort_unique(std::vector<std::string>* out) {
  std::sort(out->begin(), out->end());
  auto last = std::unique(out->begin(), out->end());
  out->erase(last, out->end());
}

namespace base {
namespace time {
namespace zone {

using ::base::backport::make_unique;

const Regime* Zone::get_regime(Time t) const {
  std::size_t i = 0;
  std::size_t j = regimes_.size();
  while (i < j) {
    std::size_t mid = (i + j) / 2;
    const Regime* ptr = &regimes_[mid];
    if (t < ptr->regime_begin()) {
      j = mid;
    } else if (t >= ptr->regime_end()) {
      i = mid + 1;
    } else {
      return ptr;
    }
  }
  return nullptr;
}

static Pointer make_utc() {
  using Mode = Recurrence::Mode;
  const Recurrence NEVER{Mode::never, 0, 0, 0, 0};
  const Recurrence ALWAYS{Mode::always, 0, 0, 0, 0};

  auto ptr = std::make_shared<Zone>();
  ptr->set_name("UTC");
  auto& types = ptr->types();
  types.emplace_back("UTC", 0, false, true);
  auto* t = &types.front();
  auto& regimes = ptr->regimes();
  regimes.emplace_back(Time::min(), Time::max(), NEVER, ALWAYS, t, t);
  return ptr;
}

static Pointer make_unknown() {
  using Mode = Recurrence::Mode;
  const Recurrence NEVER{Mode::never, 0, 0, 0, 0};
  const Recurrence ALWAYS{Mode::always, 0, 0, 0, 0};

  auto ptr = std::make_shared<Zone>();
  ptr->set_name("Unknown");
  auto& types = ptr->types();
  types.emplace_back("???", 0, false, false);
  auto* t = &types.front();
  auto& regimes = ptr->regimes();
  regimes.emplace_back(Time::min(), Time::max(), NEVER, ALWAYS, t, t);
  return ptr;
}

const Pointer& utc() {
  static const Pointer& ptr = *new Pointer(make_utc());
  return ptr;
}

const Pointer& unknown() {
  static const Pointer& ptr = *new Pointer(make_unknown());
  return ptr;
}

inline namespace implementation {
class BuiltinDatabase : public Database {
 public:
  BuiltinDatabase() noexcept = default;
  Result get(Pointer* out, StringPiece id) const override;
  Result all(std::vector<std::string>* out) const override;
};

class PosixDatabase : public Database {
 public:
  PosixDatabase() noexcept = default;
  Result get(Pointer* out, StringPiece id) const override;
  Result all(std::vector<std::string>* out) const override;
};

class ZoneInfoDatabase : public Database {
 public:
  ZoneInfoDatabase(std::unique_ptr<Loader> loader)
      : loader_(std::move(loader)) {}
  Result get(Pointer* out, StringPiece id) const override;
  Result all(std::vector<std::string>* out) const override;

 private:
  std::unique_ptr<Loader> loader_;
};

class MetaDatabase : public Database {
 public:
  MetaDatabase(std::vector<DatabasePointer> vec) noexcept
      : vec_(std::move(vec)) {}
  Result get(Pointer* out, StringPiece id) const override;
  Result all(std::vector<std::string>* out) const override;

 private:
  std::vector<DatabasePointer> vec_;
};

class CachedDatabase : public Database {
 public:
  CachedDatabase(DatabasePointer ptr) noexcept : ptr_(std::move(ptr)) {}
  Result get(Pointer* out, StringPiece id) const override;
  Result all(std::vector<std::string>* out) const override;

 private:
  using Weak = std::weak_ptr<const Zone>;
  using Cache = std::unordered_map<StringPiece, Weak>;

  DatabasePointer ptr_;
  mutable std::mutex mu_;
  mutable Cache cache_;
};

class ZoneInfoLoader : public Loader {
 public:
  ZoneInfoLoader() : dirs_(list_zoneinfo_dirs()) {}
  ZoneInfoLoader(StringPiece tzdir) : dirs_({tzdir}) {}
  Result load(std::string* out, StringPiece filename) const override;
  Result scan(std::vector<std::string>* out) const override;

 private:
  std::vector<std::string> dirs_;
};

Result BuiltinDatabase::get(Pointer* out, StringPiece id) const {
  CHECK_NOTNULL(out);
  out->reset();
  if (id == "UTC") {
    *out = utc();
    return Result();
  }
  if (id == "Unknown") {
    *out = unknown();
    return Result();
  }
  return Result::not_found();
}

Result BuiltinDatabase::all(std::vector<std::string>* out) const {
  CHECK_NOTNULL(out);
  out->push_back("UTC");
  out->push_back("Unknown");
  sort_unique(out);
  return Result();
}

Result PosixDatabase::get(Pointer* out, StringPiece id) const {
  CHECK_NOTNULL(out);
  out->reset();
  PosixRules tmp;
  auto result = parse_posix(&tmp, id);
  if (result) {
    *out = interpret_posix(tmp);
  } else if (result.code() == ResultCode::INVALID_ARGUMENT) {
    result = Result::not_found();
  }
  return result;
}

Result PosixDatabase::all(std::vector<std::string>* out) const {
  CHECK_NOTNULL(out);
  return Result();
}

Result ZoneInfoDatabase::get(Pointer* out, StringPiece id) const {
  CHECK_NOTNULL(out);
  out->reset();
  std::string data;
  id.remove_prefix(":");
  if (id.empty()) id = "localtime";
  auto result = loader_->load(&data, id);
  if (result) {
    TZifFile tmp;
    result = parse_tzif(&tmp, id, data);
    if (result) {
      *out = interpret_tzif(tmp);
    }
  }
  return result;
}

Result ZoneInfoDatabase::all(std::vector<std::string>* out) const {
  CHECK_NOTNULL(out);
  std::vector<std::string> paths;
  auto result = loader_->scan(&paths);
  if (result) {
    std::move(paths.begin(), paths.end(), std::back_inserter(*out));
    sort_unique(out);
  }
  return result;
}

Result MetaDatabase::get(Pointer* out, StringPiece id) const {
  CHECK_NOTNULL(out);
  out->reset();
  Result result;
  for (const auto& tzdb : vec_) {
    result = tzdb->get(out, id);
    if (result.code() != ResultCode::NOT_FOUND) break;
  }
  return result;
}

Result MetaDatabase::all(std::vector<std::string>* out) const {
  CHECK_NOTNULL(out);
  for (const auto& tzdb : vec_) {
    auto result = tzdb->all(out);
    if (!result) return result;
  }
  sort_unique(out);
  return Result();
}

Result CachedDatabase::get(Pointer* out, StringPiece id) const {
  CHECK_NOTNULL(out);
  out->reset();

  std::unique_lock<std::mutex> lock(mu_);
  Pointer tmp;

  // Check cache
  auto it = cache_.find(id);
  if (it != cache_.end()) {
    // Attempt to acquire strong pointer from weak
    tmp = it->second.lock();
    if (tmp) {
      // Success!
      *out = std::move(tmp);
      return Result();
    }
  }

  // Thunk, then populate cache on success
  auto result = ptr_->get(&tmp, id);
  if (result) {
    cache_[tmp->name()] = tmp;
    *out = std::move(tmp);
  }
  return result;
}

Result CachedDatabase::all(std::vector<std::string>* out) const {
  return ptr_->all(out);
}

Result ZoneInfoLoader::load(std::string* out, StringPiece filename) const {
  CHECK_NOTNULL(out);
  out->clear();

  if (!re2::RE2::FullMatch(filename, "[0-9A-Za-z]+(?:[/_.+-][0-9A-Za-z]+)*")) {
    return Result::not_found();
  }

  std::string path;
  base::FD fd;
  for (const auto& dir : dirs_) {
    path.assign(dir);
    path.push_back('/');
    filename.append_to(&path);

    int fdnum = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fdnum == -1) {
      int err_no = errno;
      if (err_no == ENOENT) continue;
      return Result::from_errno(err_no, "open: ", path);
    }
    fd = wrapfd(fdnum);
    break;
  }
  if (!fd) return Result::from_errno(ENOENT, "open: ", path);

  std::vector<char> data;
  auto result = read_all(&data, fd, path.c_str());
  if (!result) return result;

  result = fd->close();
  if (!result) return result;

  out->append(data.begin(), data.end());
  return Result();
}

static Result walk(std::vector<std::string>* out, const std::string& root,
                   const std::string& path) {
  std::string fullpath;
  fullpath.assign(root);
  if (!path.empty()) {
    fullpath.push_back('/');
    fullpath.append(path);
  }

  int fdnum = ::open(fullpath.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
  if (fdnum == -1) {
    int err_no = errno;
    if (err_no == ENOENT) return Result();
    return Result::from_errno(err_no, "open(2) path=", fullpath);
  }
  auto fd = wrapfd(fdnum);

  std::vector<DEntry> dents;
  auto result = readdir_all(&dents, fd, fullpath.c_str());
  if (!result) return result;

  for (const auto& dent : dents) {
    auto type = std::get<1>(dent);
    StringPiece name = std::get<2>(dent);

    if (name.empty() || name.front() == '.') continue;
    if (name.has_suffix(".tab")) continue;
    if (name.has_suffix(".list")) continue;
    if (name == "posixrules") continue;
    if (name == "posix") continue;
    if (name == "right") continue;

    bool is_dir = (type == DT_DIR);
    bool is_reg = (type == DT_REG);

    if (type == DT_LNK) {
      std::string itempath;
      itempath.assign(fullpath);
      itempath.push_back('/');
      name.append_to(&itempath);

      struct stat st;
      ::bzero(&st, sizeof(st));
      int rc = ::stat(itempath.c_str(), &st);
      if (rc != 0) {
        int err_no = errno;
        return Result::from_errno(err_no, "stat(2) path=", itempath);
      }
      if (S_ISDIR(st.st_mode)) {
        is_dir = true;
      } else if (S_ISREG(st.st_mode)) {
        is_reg = true;
      }
    }
    if (!is_dir && !is_reg) continue;

    std::string subpath;
    if (!path.empty()) {
      subpath.assign(path);
      subpath.push_back('/');
    }
    name.append_to(&subpath);

    if (is_dir) {
      auto result = walk(out, root, subpath);
      if (!result) return result;
    } else {
      out->push_back(subpath);
    }
  }
  return Result();
}

Result ZoneInfoLoader::scan(std::vector<std::string>* out) const {
  CHECK_NOTNULL(out);
  for (const auto& dir : dirs_) {
    auto result = walk(out, dir, "");
    if (!result) return result;
  }
  return Result();
}
}  // inline namespace implementation

DatabasePointer new_builtin_database() {
  return std::make_shared<BuiltinDatabase>();
}

DatabasePointer new_posix_database() {
  return std::make_shared<PosixDatabase>();
}

DatabasePointer new_zoneinfo_database(std::unique_ptr<Loader> loader) {
  return std::make_shared<ZoneInfoDatabase>(std::move(loader));
}

DatabasePointer new_zoneinfo_database(StringPiece tzdir) {
  return new_zoneinfo_database(make_unique<ZoneInfoLoader>(tzdir));
}

DatabasePointer new_zoneinfo_database() {
  return new_zoneinfo_database(make_unique<ZoneInfoLoader>());
}

DatabasePointer new_meta_database(std::vector<DatabasePointer> vec) {
  return std::make_shared<MetaDatabase>(std::move(vec));
}

DatabasePointer new_cached_database(DatabasePointer ptr) {
  return std::make_shared<CachedDatabase>(std::move(ptr));
}

static DatabasePointer make_system_database() {
  return new_meta_database({
      new_builtin_database(), new_posix_database(),
      new_cached_database(new_zoneinfo_database()),
  });
}

DatabasePointer system_database() {
  std::unique_lock<std::mutex> lock(g_mu);
  if (!g_sysdb) g_sysdb = new DatabasePointer;
  if (!*g_sysdb) *g_sysdb = make_system_database();
  return *g_sysdb;
}

void set_system_database(DatabasePointer tzdb) {
  std::unique_lock<std::mutex> lock(g_mu);
  if (!g_sysdb) g_sysdb = new DatabasePointer;
  *g_sysdb = std::move(tzdb);
}

std::string format_offset(int32_t offset, bool use_zulu) {
  char sign;
  uint32_t h, m, s;
  if (use_zulu && offset == 0) {
    return "Z";
  } else if (offset >= 0) {
    sign = '+';
    s = static_cast<uint32_t>(offset);
  } else {
    sign = '-';
    s = static_cast<uint32_t>(-(offset + 1)) + 1;
  }
  h = (s / 3600);
  s %= 3600;
  m = (s / 60);
  s %= 60;

  std::ostringstream o;
  o << std::setfill('0');
  o << sign << std::setw(2) << h << ':' << std::setw(2) << m;
  if (s) o << ':' << std::setw(2) << s;
  return o.str();
}

}  // namespace zone
}  // namespace time
}  // namespace base
