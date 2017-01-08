// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/result_testing.h"
#include "file/mem.h"

static const char kHelloWorld[] = "Hello, world!\n";

static base::Result prepare_tree(file::FileSystemPtr fs, base::Options opts) {
  event::Task task;
  file::File f;
  base::Result r;

  opts.get<file::Options>().create_perm = 0666;
  opts.get<file::Options>().create_dir_perm = 0777;
  opts.get<file::Options>().perm_mask = 0;
  opts.get<file::Options>().user = "root";
  opts.get<file::Options>().group = "root";
  opts.get<file::Options>().open_directory = true;

  fs->open(&task, &f, "/foo", file::Mode::create_exclusive_wo_mode(), opts);
  event::wait(io::get_manager(opts), &task);
  r = task.result();
  if (!r) return r;

  r = f.close(opts);
  if (!r) return r;

  task.reset();
  fs->open(&task, &f, "/quux", file::Mode::create_exclusive_wo_mode(), opts);
  event::wait(io::get_manager(opts), &task);
  r = task.result();
  if (!r) return r;

  r = f.close(opts);
  if (!r) return r;

  opts.get<file::Options>().open_directory = false;

  task.reset();
  fs->open(&task, &f, "/foo/bar", file::Mode::create_exclusive_wo_mode(), opts);
  event::wait(io::get_manager(opts), &task);
  r = task.result();
  if (!r) return r;

  std::size_t n;
  r = f.writer().write(&n, kHelloWorld, sizeof(kHelloWorld) - 1, opts);
  if (!r) return r;

  return f.close(opts);
}

TEST(MemFS, Stat) {
  base::Options opts;
  auto fs = file::mem_filesystem("test-memfs-stat");
  ASSERT_OK(prepare_tree(fs, opts));

  file::Stat st;
  auto stat = [&opts, &fs, &st](const std::string& path) {
    event::Task task;
    fs->stat(&task, &st, path, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  };

  EXPECT_OK(stat("/"));
  EXPECT_EQ(file::FileType::directory, st.type);
  EXPECT_EQ("root", st.owner);
  EXPECT_EQ("root", st.group);
  EXPECT_EQ(0U, st.size);
  EXPECT_EQ(0U, st.size_blocks);

  EXPECT_OK(stat("/foo/bar"));
  EXPECT_EQ(file::FileType::regular, st.type);
  EXPECT_EQ(14U, st.size);
  EXPECT_EQ(1U, st.size_blocks);
  EXPECT_EQ(4096U, st.optimal_block_size);
}

TEST(MemFS, Open) {
  base::Options opts;
  auto fs = file::mem_filesystem("test-memfs-open");
  ASSERT_OK(prepare_tree(fs, opts));

  opts.get<file::Options>().create_perm = 0640;
  opts.get<file::Options>().create_dir_perm = 0700;

  file::File f;
  auto open = [&opts, &fs, &f](const std::string& path, file::Mode mode) {
    event::Task task;
    fs->open(&task, &f, path, mode, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  };

  char buf[32];
  std::size_t n;

  EXPECT_OK(open("/foo/bar", file::Mode::ro_mode()));
  EXPECT_OK(f.reader().read(buf, &n, 0, sizeof(buf), opts));
  EXPECT_EQ(kHelloWorld, std::string(buf, n));
  EXPECT_PERMISSION_DENIED(f.writer().write(&n, ""));
  EXPECT_OK(f.close());

  EXPECT_NOT_FOUND(open("/foo/baz", file::Mode::rw_mode()));

  EXPECT_OK(open("/foo/baz", file::Mode::create_exclusive_wo_mode()));
  EXPECT_OK(f.writer().write(&n, "abc\n"));
  EXPECT_OK(f.close());

  EXPECT_ALREADY_EXISTS(open("/foo/baz", file::Mode::create_exclusive_wo_mode()));

  EXPECT_OK(open("/foo/baz", file::Mode::create_truncate_rw_mode()));
  EXPECT_OK(f.reader().read(buf, &n, 0, sizeof(buf), opts));
  EXPECT_EQ("", std::string(buf, n));
  EXPECT_OK(f.close());

  EXPECT_WRONG_TYPE(open("/", file::Mode::ro_mode()));

  opts.get<file::Options>().open_directory = true;

  EXPECT_WRONG_TYPE(open("/foo/bar", file::Mode::ro_mode()));

  EXPECT_OK(open("/", file::Mode::ro_mode()));
  std::vector<file::DirEntry> vec;
  EXPECT_OK(f.readdir(&vec, opts));
  EXPECT_EQ(2U, vec.size());
  if (vec.size() > 0U) {
    EXPECT_EQ("foo", vec[0].name);
    EXPECT_EQ(file::FileType::directory, vec[0].type);
  }
  if (vec.size() > 1U) {
    EXPECT_EQ("quux", vec[1].name);
    EXPECT_EQ(file::FileType::directory, vec[1].type);
  }
  EXPECT_OK(f.close());

  EXPECT_OK(open("/foo", file::Mode::ro_mode()));
  EXPECT_OK(f.readdir(&vec, opts));
  EXPECT_EQ(2U, vec.size());
  if (vec.size() > 0U) {
    EXPECT_EQ("bar", vec[0].name);
    EXPECT_EQ(file::FileType::regular, vec[0].type);
  }
  if (vec.size() > 1U) {
    EXPECT_EQ("baz", vec[1].name);
    EXPECT_EQ(file::FileType::regular, vec[1].type);
  }
}

TEST(MemFS, MkDir) {
  base::Options opts;
  auto fs = file::mem_filesystem("test-memfs-mkdir");
  ASSERT_OK(prepare_tree(fs, opts));

  opts.get<file::Options>().create_perm = 0640;
  opts.get<file::Options>().create_dir_perm = 0700;
  opts.get<file::Options>().open_directory = true;

  file::Stat st;
  auto stat = [&opts, &fs, &st](const std::string& path) {
    event::Task task;
    fs->stat(&task, &st, path, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  };

  auto mkdir = [&opts, &fs](const std::string& path) {
    event::Task task;
    file::File f;
    fs->open(&task, &f, path, file::Mode::create_exclusive_wo_mode(), opts);
    event::wait(io::get_manager(opts), &task);
    base::Result r = task.result();
    if (r) r = f.close(opts);
    return r;
  };

  EXPECT_OK(stat("/foo"));
  EXPECT_EQ(2U, st.link_count);

  EXPECT_NOT_FOUND(stat("/foo/baz"));

  EXPECT_OK(mkdir("/foo/baz"));

  EXPECT_OK(stat("/foo/baz"));
  EXPECT_EQ(file::FileType::directory, st.type);
  EXPECT_EQ(2U, st.link_count);
  EXPECT_EQ(0700, uint16_t(st.perm));

  EXPECT_OK(stat("/foo"));
  EXPECT_EQ(3U, st.link_count);

  EXPECT_ALREADY_EXISTS(mkdir("/foo/baz"));
}

TEST(MemFile, EndToEnd) {
  base::Options opts;
  auto fs = file::mem_filesystem("test-memfile-e2e");
  ASSERT_OK(prepare_tree(fs, opts));

  const auto mode = file::Mode::create_exclusive_rw_mode();
  event::Task task;
  file::File f;
  fs->open(&task, &f, "/foo/baz", mode, opts);
  event::wait(io::get_manager(opts), &task);
  EXPECT_OK(task.result());

  char buf[32];
  std::size_t n;
  int64_t off;

  auto read_all = [&opts, &f, &buf, &n, &off]() {
    std::string out;
    CHECK_OK(f.tell(&off, opts));
    CHECK_OK(f.seek(0, file::Whence::start, opts));
    while (true) {
      CHECK_OK(f.reader().read(buf, &n, 0, sizeof(buf), opts));
      out.append(buf, n);
      if (n == 0) break;
    }
    CHECK_OK(f.seek(off, file::Whence::start, opts));
    return out;
  };

  EXPECT_OK(f.size(&off, opts));
  EXPECT_EQ(0, off);
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(0, off);
  EXPECT_EQ("", read_all());

  EXPECT_OK(f.writer().write(&n, "abc", 3, opts));
  EXPECT_EQ(3U, n);
  EXPECT_OK(f.size(&off, opts));
  EXPECT_EQ(3, off);
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(3, off);
  EXPECT_EQ("abc", read_all());

  EXPECT_OK(f.writer().write(&n, "defg", 4, opts));
  EXPECT_EQ(4U, n);
  EXPECT_OK(f.size(&off, opts));
  EXPECT_EQ(7, off);
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(7, off);
  EXPECT_EQ("abcdefg", read_all());

  EXPECT_OK(f.seek(-2, file::Whence::current, opts));
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(5, off);
  EXPECT_OK(f.writer().write(&n, "hijk", 4, opts));
  EXPECT_EQ(4U, n);
  EXPECT_OK(f.size(&off, opts));
  EXPECT_EQ(9, off);
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(9, off);
  EXPECT_EQ("abcdehijk", read_all());

  EXPECT_OK(f.seek(3, file::Whence::start, opts));
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(3, off);
  EXPECT_OK(f.writer().write(&n, "lm", 2, opts));
  EXPECT_EQ(2U, n);
  EXPECT_OK(f.size(&off, opts));
  EXPECT_EQ(9, off);
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(5, off);
  EXPECT_EQ("abclmhijk", read_all());

  EXPECT_OK(f.reader().read(buf, &n, 2, 2, opts));
  EXPECT_EQ(2U, n);
  EXPECT_EQ("hi", std::string(buf, n));
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(7, off);

  EXPECT_EOF(f.reader().read(buf, &n, 4, 4, opts));
  EXPECT_EQ(2U, n);
  EXPECT_EQ("jk", std::string(buf, n));
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(9, off);

  EXPECT_OK(f.reader().read(buf, &n, 0, 0, opts));
  EXPECT_EQ(0U, n);
  EXPECT_OK(f.tell(&off, opts));
  EXPECT_EQ(9, off);
}
