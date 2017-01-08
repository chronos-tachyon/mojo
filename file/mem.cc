// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/mem.h"

#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "base/cleanup.h"
#include "base/clock.h"
#include "base/mutex.h"
#include "file/fs.h"
#include "file/registry.h"
#include "io/reader.h"
#include "io/writer.h"
#include "path/path.h"

namespace file {

namespace {

static constexpr uint64_t S64MAX = std::numeric_limits<int64_t>::max();
static constexpr uint64_t S64MAX_PLUS_1 = S64MAX + 1;

static base::Result not_a_directory() {
  return base::Result::wrong_type("not a directory");
}

static base::Result is_a_directory() {
  return base::Result::wrong_type("is a directory");
}

static base::Result parent_not_writable() {
  return base::Result::permission_denied("parent directory not writable");
}

static base::Result inode_not_readable() {
  return base::Result::permission_denied("no read permission");
}

static base::Result inode_not_writable() {
  return base::Result::permission_denied("no write permission");
}

static base::Result file_closed() {
  return base::Result::from_errno(EBADF, "file::File is closed");
}

static base::Result no_read() {
  return base::Result::permission_denied("file not open for read");
}

static base::Result no_write() {
  return base::Result::permission_denied("file not open for write");
}

struct Inode;

using InodePtr = std::shared_ptr<Inode>;
using Tuple = std::tuple<std::string, InodePtr, InodePtr>;

struct Inode {
  using Bytes = std::vector<char>;
  using DEntryMap = std::map<std::string, InodePtr>;

  const FileType type;
  mutable std::mutex mu;
  Perm perm;
  std::string owner;
  std::string group;
  std::size_t nlinks;
  base::Time create_time;
  base::Time change_time;
  base::Time modify_time;
  base::Time access_time;
  Bytes data;
  DEntryMap dentries;

  Inode(FileType type, Perm perm, std::string owner, std::string group)
      : type(type),
        perm(perm),
        owner(std::move(owner)),
        group(std::move(group)),
        nlinks(0),
        create_time(base::now()),
        change_time(create_time),
        modify_time(create_time),
        access_time(create_time) {}

  bool is_directory() const noexcept { return type == FileType::directory; }
  UserPerm role(const file::Options& fo) const;
  base::Result stat(Stat* out) const;

  static InodePtr make_regular(const file::Options& fo) {
    auto perm = fo.masked_create_perm();
    return std::make_shared<Inode>(FileType::regular, perm, fo.user, fo.group);
  }

  static InodePtr make_directory(const file::Options& fo) {
    auto perm = fo.masked_create_dir_perm();
    return std::make_shared<Inode>(FileType::directory, perm, fo.user,
                                   fo.group);
  }

  static InodePtr make_root() {
    return std::make_shared<Inode>(FileType::directory, 0777, "root", "root");
  }
};

struct Resolver {
  const InodePtr root;
  const std::string path;
  const file::Options& fileoptions;
  const bool missing_ok;
  std::vector<Tuple> stack;
  std::string canonical;
  bool trailing_slashes;

  Resolver(InodePtr root, const std::string& path, const file::Options& fo,
           bool missing_ok)
      : root(std::move(root)),
        path(path::partial_clean(path)),
        fileoptions(fo),
        missing_ok(missing_ok),
        trailing_slashes(false) {}
  void populate_canonical();
  base::Result run();
};

struct Descriptor {
  InodePtr inode;
  std::size_t pos;
  Mode mode;
  bool closed;

  Descriptor(InodePtr inode, Mode mode) noexcept
      : inode(CHECK_NOTNULL(std::move(inode))),
        pos(0),
        mode(mode),
        closed(false) {}
};

class DescriptorReader : public io::ReaderImpl {
 public:
  DescriptorReader(Descriptor* d) noexcept : desc_(*CHECK_NOTNULL(d)) {}

  std::size_t ideal_block_size() const noexcept override {
    return io::kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override;

  void close(event::Task* task, const base::Options& opts) override;

  static io::Reader make(Descriptor* d) {
    return io::Reader(std::make_shared<DescriptorReader>(d));
  }

 private:
  Descriptor& desc_;
};

class DescriptorWriter : public io::WriterImpl {
 public:
  DescriptorWriter(Descriptor* d) noexcept : desc_(*CHECK_NOTNULL(d)) {}

  std::size_t ideal_block_size() const noexcept override {
    return io::kDefaultIdealBlockSize;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override;

  void close(event::Task* task, const base::Options& opts) override;

  static io::Writer make(Descriptor* d) {
    return io::Writer(std::make_shared<DescriptorWriter>(d));
  }

 private:
  Descriptor& desc_;
};

class MemFS : public FileSystemImpl {
 public:
  explicit MemFS(std::string name, InodePtr root)
      : FileSystemImpl(std::move(name)), root_(CHECK_NOTNULL(std::move(root))) {
    CHECK_EQ(root_->type, FileType::directory);
    auto lock = base::acquire_lock(root_->mu);
    root_->nlinks++;
  }

  ~MemFS() noexcept override {
    auto lock = base::acquire_lock(root_->mu);
    root_->nlinks--;
  }

  void statfs(event::Task* task, StatFS* out, const std::string& path,
              const base::Options& opts) const override;

  void stat(event::Task* task, Stat* out, const std::string& path,
            const base::Options& opts) const override;

  void set_stat(event::Task* task, const std::string& path,
                const SetStat& delta, const base::Options& opts) override;

  void open(event::Task* task, File* out, const std::string& path, Mode mode,
            const base::Options& opts) override;

  void link(event::Task* task, const std::string& oldpath,
            const std::string& newpath, const base::Options& opts) override;

  void symlink(event::Task* task, const std::string& target,
               const std::string& linkpath, const base::Options& opts) override;

  void unlink(event::Task* task, const std::string& path,
              const base::Options& opts) override;

  static FileSystemPtr make(std::string name) {
    return std::make_shared<MemFS>(std::move(name), Inode::make_root());
  }

 private:
  InodePtr root_;
};

class MemFile : public FileImpl {
 public:
  MemFile(FileSystemPtr fs, std::string path, Mode mode, InodePtr inode)
      : FileImpl(std::move(fs), std::move(path), mode),
        desc_(std::move(inode), mode),
        r_(DescriptorReader::make(&desc_)),
        w_(DescriptorWriter::make(&desc_)) {}

  io::Reader reader() override { return r_; }
  io::Writer writer() override { return w_; }

  void readdir(event::Task* task, std::vector<DirEntry>* out,
               const base::Options& opts) const override;

  void statfs(event::Task* task, StatFS* out,
              const base::Options& opts) const override;

  void stat(event::Task* task, Stat* out,
            const base::Options& opts) const override;

  void size(event::Task* task, int64_t* out,
            const base::Options& opts) const override;

  void tell(event::Task* task, int64_t* out,
            const base::Options& opts) const override;

  void set_stat(event::Task* task, const SetStat& delta,
                const base::Options& opts) override;

  void seek(event::Task* task, int64_t off, Whence whence,
            const base::Options& opts) override;

  void truncate_at(event::Task* task, int64_t off,
                   const base::Options& opts) override;

  void close(event::Task* task, const base::Options& opts) override;

  static File make(FileSystemPtr fs, std::string path, Mode mode,
                   InodePtr inode) {
    return File(std::make_shared<MemFile>(std::move(fs), std::move(path), mode,
                                          std::move(inode)));
  }

 private:
  Descriptor desc_;
  io::Reader r_;
  io::Writer w_;
};

UserPerm Inode::role(const file::Options& fo) const {
  if (fo.user == owner)
    return perm.user();
  else if (fo.group == group)
    return perm.group();
  else
    return perm.other();
}

base::Result Inode::stat(Stat* out) const {
  Stat tmp;
  tmp.type = type;
  tmp.perm = perm;
  tmp.owner = owner;
  tmp.group = group;
  tmp.link_count = nlinks;
  tmp.size = data.size();
  tmp.size_blocks = (tmp.size + 511) / 512;
  tmp.optimal_block_size = 4096;
  tmp.create_time = create_time;
  tmp.change_time = change_time;
  tmp.modify_time = modify_time;
  tmp.access_time = access_time;
  *out = tmp;
  return base::Result();
}

void Resolver::populate_canonical() {
  auto it = stack.begin(), end = stack.end();
  ++it;
  if (it == end) canonical.push_back('/');
  while (it != end) {
    canonical.push_back('/');
    canonical.append(std::get<0>(*it));
    ++it;
  }
}

base::Result Resolver::run() {
  if (path.empty() || path.front() != '/')
    return base::Result::invalid_argument("requires a rooted path");

  const char* ptr = path.data();
  const char* end = ptr + path.size();

  // Advance ptr past the initial slash(es)
  //
  //    "/foo/bar"
  //     ^^
  //     |+- ptr
  //     +-- path.data()
  //
  while (ptr != end && *ptr == '/') ++ptr;
  stack.emplace_back("/", nullptr, root);
  trailing_slashes = true;

  while (ptr != end) {
    // Find the next slash:
    //
    //    "/foo/bar"
    //      ^  ^
    //      |  +- slash
    //      +---- ptr
    //
    // Alternatively, find the end of the string:
    //
    //    "/foo/bar"
    //          ^  ^
    //          |  +- slash
    //          +---- ptr
    //
    const char* slash = ptr;
    while (slash != end && *slash != '/') ++slash;

    // The char range ptr..slash is the current path component.
    std::string name(ptr, slash);

    // Position ptr to next path component, if any.
    ptr = slash;
    while (ptr != end && *ptr == '/') ++ptr;

    // Did we find any trailing slashes?
    // If so, the dentry we're looking up had better be a directory.
    trailing_slashes = (slash != end);

    // Did we hit end-of-string (maybe after some trailing slashes)?
    // If so, we're allowed to push a non-directory onto the stack.
    bool end_of_string = (ptr == end);

    // Ignore "."
    if (name == ".") continue;

    // Pop the stack on seeing ".."
    if (name == "..") {
      if (stack.size() == 1) {
        populate_canonical();
        return base::Result::invalid_argument(
            "cannot .. past the root directory");
      }
      stack.pop_back();
      continue;
    }

    // We got something else.  Look it up in the topmost inode.
    // INVARIANT: while in the loop, the topmost inode is a directory.
    auto& inode = std::get<2>(stack.back());
    auto lock = base::acquire_lock(inode->mu);

    if (!inode->role(fileoptions).exec())
      return base::Result::permission_denied("parent directory not searchable");

    auto& dentries = inode->dentries;
    auto it = dentries.find(name);

    // CASE 1: found something
    if (it != dentries.end()) {
      auto& child = it->second;
      stack.emplace_back(name, inode, child);
      if (trailing_slashes && !child->is_directory()) {
        populate_canonical();
        return not_a_directory();
      }
      continue;
    }

    // CASE 2a: found nothing, and there's more path to resolve
    if (!end_of_string) {
      populate_canonical();
      return base::Result::not_found("missing parent directory");
    }

    // CASE 2b: found nothing, but the path was otherwise resolved
    stack.emplace_back(name, inode, nullptr);
    if (!missing_ok) {
      populate_canonical();
      return base::Result::not_found();
    }
  }

  populate_canonical();
  return base::Result();
}

void DescriptorReader::read(event::Task* task, char* out, std::size_t* n,
                            std::size_t min, std::size_t max,
                            const base::Options& opts) {
  if (!prologue(task, out, n, min, max)) return;

  auto lock = base::acquire_lock(desc_.inode->mu);

  if (desc_.closed) {
    task->finish(io::reader_closed());
    return;
  }

  if (!desc_.mode.read()) {
    task->finish(no_read());
    return;
  }

  auto& buf = desc_.inode->data;
  auto& pos = desc_.pos;
  std::size_t len = 0;
  if (buf.size() > pos) len = buf.size() - pos;
  if (len > max) len = max;
  ::memcpy(out, buf.data() + pos, len);
  pos += len;
  desc_.inode->access_time = base::now();

  lock.unlock();

  *n = len;
  if (min > len)
    task->finish(base::Result::eof());
  else
    task->finish_ok();
}

void DescriptorReader::close(event::Task* task, const base::Options& opts) {
  auto lock = base::acquire_lock(desc_.inode->mu);
  bool was = desc_.closed;
  desc_.closed = true;
  lock.unlock();

  if (prologue(task)) {
    if (was)
      task->finish(io::reader_closed());
    else
      task->finish_ok();
  }
}

void DescriptorWriter::write(event::Task* task, std::size_t* n, const char* ptr,
                             std::size_t len, const base::Options& opts) {
  if (!prologue(task, n, ptr, len)) return;

  auto lock = base::acquire_lock(desc_.inode->mu);

  if (desc_.closed) {
    task->finish(io::writer_closed());
    return;
  }

  if (!desc_.mode.write()) {
    task->finish(no_write());
    return;
  }

  auto& buf = desc_.inode->data;
  auto& pos = desc_.pos;
  if (desc_.mode.append()) pos = buf.size();
  if (pos + len > buf.size()) buf.resize(pos + len);
  ::memcpy(buf.data() + pos, ptr, len);
  pos += len;
  auto now = base::now();
  desc_.inode->modify_time = now;
  desc_.inode->access_time = now;

  lock.unlock();

  *n = len;
  task->finish_ok();
}

void DescriptorWriter::close(event::Task* task, const base::Options& opts) {
  auto lock = base::acquire_lock(desc_.inode->mu);
  bool was = desc_.closed;
  desc_.closed = true;
  lock.unlock();

  if (prologue(task)) {
    if (was)
      task->finish(io::writer_closed());
    else
      task->finish_ok();
  }
}

void MemFS::statfs(event::Task* task, StatFS* out, const std::string& path,
                   const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = StatFS();
  task->finish(base::Result::not_implemented());  // FIXME
}

void MemFS::stat(event::Task* task, Stat* out, const std::string& path,
                 const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = Stat();

  InodePtr inode;
  {
    Resolver resolver(root_, path, opts, false);
    base::Result r = resolver.run();
    if (!r) {
      task->finish(std::move(r));
      return;
    }
    std::tie(std::ignore, std::ignore, inode) = resolver.stack.back();
  }

  auto lock = base::acquire_lock(inode->mu);
  inode->stat(out);
  task->finish_ok();
}

void MemFS::set_stat(event::Task* task, const std::string& path,
                     const SetStat& delta, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  task->finish(base::Result::not_implemented());  // FIXME
}

void MemFS::open(event::Task* task, File* out, const std::string& path,
                 Mode mode, const base::Options& opts) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = File();

  if (!mode.valid()) {
    task->finish(base::Result::invalid_argument("nonsensical mode"));
    return;
  }

  std::string name;
  InodePtr parent;
  InodePtr inode;
  std::string canon;
  bool trailing_slashes;
  {
    Resolver resolver(root_, path, opts, mode.create());
    base::Result r = resolver.run();
    if (!r) {
      task->finish(std::move(r));
      return;
    }
    std::tie(name, parent, inode) = resolver.stack.back();
    canon = std::move(resolver.canonical);
    trailing_slashes = resolver.trailing_slashes;
  }

  const file::Options& fo = opts;
  if (trailing_slashes && !fo.open_directory) {
    task->finish(not_a_directory());
    return;
  }

  bool created = false;
  if (!inode) {
    auto lock = base::acquire_lock(parent->mu);

    if (!parent->role(fo).write()) {
      task->finish(parent_not_writable());
      return;
    }

    auto& slot = parent->dentries[name];  // provoke exceptions
    if (fo.open_directory) {
      inode = file::Inode::make_directory(fo);
      inode->nlinks += 2;  // parent -> inode and '.' -> inode
      parent->nlinks++;    // '..' -> parent
    } else {
      inode = file::Inode::make_regular(fo);
      inode->nlinks++;  // just parent -> inode
    }
    slot = inode;
    created = true;
  }

  if (mode.exclusive() && !created) {
    task->finish(base::Result::already_exists());
    return;
  }

  if (fo.open_directory) {
    if (!inode->is_directory()) {
      task->finish(not_a_directory());
      return;
    }
  } else {
    if (inode->is_directory()) {
      task->finish(is_a_directory());
      return;
    }
  }

  auto lock = base::acquire_lock(inode->mu);

  if (mode.read() && !inode->role(fo).read()) {
    task->finish(inode_not_readable());
    return;
  }

  if (mode.write() && !inode->role(fo).write()) {
    task->finish(inode_not_writable());
    return;
  }

  if (mode.truncate()) inode->data.clear();

  *out = MemFile::make(self(), canon, mode, std::move(inode));
  task->finish_ok();
}

void MemFS::link(event::Task* task, const std::string& oldpath,
                 const std::string& newpath, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  task->finish(base::Result::not_implemented());  // FIXME
}

void MemFS::symlink(event::Task* task, const std::string& target,
                    const std::string& linkpath, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  task->finish(base::Result::not_implemented());  // FIXME
}

void MemFS::unlink(event::Task* task, const std::string& path,
                   const base::Options& opts) {
  const file::Options& fo = opts;
  std::string name;
  InodePtr parent;
  InodePtr inode;
  {
    Resolver resolver(root_, path, fo, false);
    base::Result r = resolver.run();
    if (!r) {
      task->finish(std::move(r));
      return;
    }
    if (resolver.stack.size() == 1) {
      task->finish(
          base::Result::invalid_argument("cannot unlink the root directory"));
      return;
    }
    std::tie(name, parent, inode) = resolver.stack.back();
  }

  if (fo.remove_directory) {
    if (!inode->is_directory()) {
      task->finish(not_a_directory());
      return;
    }
  } else {
    if (inode->is_directory()) {
      task->finish(is_a_directory());
      return;
    }
  }

  auto lock0 = base::acquire_lock(parent->mu);
  auto lock1 = base::acquire_lock(inode->mu);

  if (!parent->role(fo).write()) {
    task->finish(parent_not_writable());
    return;
  }

  if (!inode->dentries.empty()) {
    task->finish(base::Result::failed_precondition("directory not empty"));
    return;
  }

  auto& dentries = parent->dentries;
  auto it = dentries.find(name);
  if (it == dentries.end()) {
    // lost a race
    task->finish(base::Result::not_found());
    return;
  }
  if (it->second != inode) {
    // lost a race
    task->finish(base::Result::not_found());
    return;
  }

  dentries.erase(it);
  inode->nlinks--;                              // parent -> inode
  if (inode->is_directory()) parent->nlinks--;  // '..' -> parent
  task->finish_ok();
}

void MemFile::readdir(event::Task* task, std::vector<DirEntry>* out,
                      const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  if (!mode().read()) {
    task->finish(no_read());
    return;
  }
  if (!desc_.inode->is_directory()) {
    task->finish(not_a_directory());
    return;
  }
  for (const auto& pair : desc_.inode->dentries) {
    out->emplace_back(pair.first, pair.second->type);
  }
  task->finish_ok();
}

void MemFile::statfs(event::Task* task, StatFS* out,
                     const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = StatFS();
  task->finish(base::Result::not_implemented());  // FIXME
}

void MemFile::stat(event::Task* task, Stat* out,
                   const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = Stat();

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  desc_.inode->stat(out);
  task->finish_ok();
}

void MemFile::size(event::Task* task, int64_t* out,
                   const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  *out = desc_.inode->data.size();
  task->finish_ok();
}

void MemFile::tell(event::Task* task, int64_t* out,
                   const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  if (desc_.inode->is_directory()) {
    task->finish(is_a_directory());
    return;
  }
  if (desc_.pos > S64MAX) {
    task->finish(
        base::Result::out_of_range("position is too large to represent"));
    return;
  }
  *out = desc_.pos;
  task->finish_ok();
}

void MemFile::set_stat(event::Task* task, const SetStat& delta,
                       const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  task->finish(base::Result::not_implemented());  // FIXME
}

void MemFile::seek(event::Task* task, int64_t off, Whence whence,
                   const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  if (desc_.inode->is_directory()) {
    task->finish(is_a_directory());
    return;
  }
  CHECK_LE(desc_.pos, S64MAX_PLUS_1);
  std::size_t base;
  bool found = false;
  switch (whence) {
    case Whence::start:
      CHECK_GE(off, 0);
      base = 0;
      found = true;
      break;

    case Whence::current:
      base = desc_.pos;
      found = true;
      break;

    case Whence::end:
      base = desc_.inode->data.size();
      found = true;
      break;
  }
  CHECK(found) << ": unknown file::Whence value";
  if (off >= 0) {
    if (uint64_t(off) > S64MAX_PLUS_1 - base) {
      task->finish(
          base::Result::out_of_range("position is beyond range of int64_t"));
      return;
    }
    base += uint64_t(off);
  } else {
    off = -off;
    if (uint64_t(off) > base) {
      task->finish(
          base::Result::out_of_range("position is before start of file"));
      return;
    }
    base -= uint64_t(off);
  }
  CHECK_LE(base, S64MAX_PLUS_1);
  desc_.pos = base;
  task->finish_ok();
}

void MemFile::truncate_at(event::Task* task, int64_t off,
                          const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  if (off < 0) {
    task->finish(base::Result::out_of_range("off < 0"));
    return;
  }

  auto lock = base::acquire_lock(desc_.inode->mu);
  if (desc_.closed) {
    task->finish(file_closed());
    return;
  }
  if (desc_.inode->is_directory()) {
    task->finish(is_a_directory());
    return;
  }
  if (!mode().write()) {
    task->finish(no_write());
    return;
  }
  desc_.inode->data.resize(uint64_t(off));
  task->finish_ok();
}

void MemFile::close(event::Task* task, const base::Options& opts) {
  CHECK_NOTNULL(task);

  auto lock = base::acquire_lock(desc_.inode->mu);
  bool was = desc_.closed;
  desc_.closed = true;
  lock.unlock();

  if (task->start()) {
    if (was)
      task->finish(file_closed());
    else
      task->finish_ok();
  }
}

using Map = std::map<std::string, std::weak_ptr<FileSystemImpl>>;

static std::mutex g_mu;
static Map* g_map = nullptr;

}  // anonymous namespace

FileSystemPtr mem_filesystem(std::string name) {
  FileSystemPtr ptr;
  auto lock = base::acquire_lock(g_mu);
  if (!g_map) g_map = new Map;
  auto it = g_map->find(name);
  if (it != g_map->end()) {
    ptr = it->second.lock();
  }
  if (!ptr) {
    ptr = MemFS::make(name);
    (*g_map)[name] = ptr;
  }
  return ptr;
}

}  // namespace file

static void init() __attribute__((constructor));
static void init() {
  auto lock = base::acquire_lock(file::system_registry_mutex());
  file::system_registry_mutable().add(nullptr, 50, file::mem_filesystem("mem"));
}
