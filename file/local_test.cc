// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#include "base/cleanup.h"
#include "base/fd.h"
#include "base/result_testing.h"
#include "base/user.h"
#include "file/fd.h"
#include "file/file.h"
#include "file/local.h"

TEST(LocalFS, Linker) {
  auto fs = file::system_registry().find("local");
  EXPECT_TRUE(!!fs);
}

TEST(LocalFS, Basics) {
  auto fs = file::local_filesystem();
  EXPECT_TRUE(!!fs);
  EXPECT_EQ("local", fs->name());

  event::Task task;
  base::Options opts;
  file::StatFS statfs;
  file::Stat stat;
  file::File f;

  fs->statfs(&task, &statfs, "/", opts);
  event::wait(io::get_manager(opts), &task);
  EXPECT_OK(task.result());
  std::cout << statfs << std::endl;

  task.reset();
  fs->stat(&task, &stat, "/dev/null", opts);
  event::wait(io::get_manager(opts), &task);
  EXPECT_OK(task.result());
  std::cout << stat << std::endl;
  EXPECT_EQ(file::FileType::char_device, stat.type);
  EXPECT_EQ(0666, uint16_t(stat.perm));

  task.reset();
  fs->stat(&task, &stat, "/dev/fd", opts);
  event::wait(io::get_manager(opts), &task);
  EXPECT_OK(task.result());
  std::cout << stat << std::endl;
  EXPECT_EQ(file::FileType::directory, stat.type);

  task.reset();
  opts.get<file::Options>().nofollow = true;
  fs->stat(&task, &stat, "/dev/fd", opts);
  event::wait(io::get_manager(opts), &task);
  EXPECT_OK(task.result());
  std::cout << stat << std::endl;
  EXPECT_EQ(file::FileType::symbolic_link, stat.type);

  task.reset();
  fs->open(&task, &f, "/dev/null", file::Mode::ro_mode(), opts);
  event::wait(io::get_manager(opts), &task);
  EXPECT_OK(task.result());

  if (f) {
    std::string out;
    EXPECT_EOF(f.reader().read(&out, 16));
    EXPECT_EQ("", out);
    EXPECT_OK(f.close());
  }
}

TEST(FDFile, EndToEnd) {
  std::string dir;
  ASSERT_OK(base::make_tempdir(&dir, "mojo2_file_fd_XXXXXXXX"));
  auto cleanup = base::cleanup([&dir] { ::rmdir(dir.c_str()); });

  std::string path = dir;
  path.append("/foo");

  auto fs = file::local_filesystem();

  const int32_t my_uid = ::getuid();
  const int32_t my_gid = ::getgid();

  base::User user;
  base::Group group;
  EXPECT_OK(base::user_by_id(&user, my_uid));
  EXPECT_OK(base::group_by_id(&group, my_gid));

  base::Options opts;
  opts.get<file::Options>().create_perm = 0666;
  opts.get<file::Options>().perm_mask = 077;
  event::Task task;
  file::File f;
  const auto mode = file::Mode::create_exclusive_rw_mode();
  fs->open(&task, &f, path, mode, opts);
  event::wait(io::get_manager(opts), &task);
  ASSERT_OK(task.result());

  EXPECT_TRUE(f.filesystem() == fs);
  EXPECT_EQ(path, f.path());
  EXPECT_EQ(mode, f.mode());

  file::StatFS statfs;
  EXPECT_OK(f.statfs(&statfs, opts));
  EXPECT_GE(statfs.optimal_block_size, 512U);
  EXPECT_NE(statfs.used_inodes, 0U);

  file::Stat stat;
  EXPECT_OK(f.stat(&stat, opts));
  EXPECT_EQ(file::FileType::regular, stat.type);
  EXPECT_EQ(user.name, stat.owner);
  EXPECT_EQ(group.name, stat.group);
  EXPECT_EQ(0600, uint16_t(stat.perm));

  int64_t pos = -1;
  EXPECT_OK(f.tell(&pos, opts));
  EXPECT_EQ(0, pos);

  std::array<char, 64> buf;
  buf.fill('A');
  std::size_t n;
  EXPECT_OK(f.writer().write(&n, buf.data(), buf.size(), opts));
  EXPECT_EQ(64U, n);

  EXPECT_OK(f.tell(&pos, opts));
  EXPECT_EQ(64, pos);

  EXPECT_OK(f.seek(-32, file::Whence::current, opts));

  buf.fill('B');
  EXPECT_OK(f.reader().read(buf.data(), &n, 1, buf.size(), opts));
  EXPECT_EQ(32U, n);
  EXPECT_EQ(std::string(32, 'A'), std::string(buf.data(), n));

  int64_t sz = -1;
  EXPECT_OK(f.size(&sz, opts));
  EXPECT_EQ(64, sz);

  EXPECT_OK(f.close(opts));
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
