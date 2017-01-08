// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "file/perm.h"

TEST(UserPerm, Basics) {
  struct TestItem {
    uint8_t bits;
    bool has_s;
    bool has_r;
    bool has_w;
    bool has_x;
    std::string str;
  };

  std::vector<TestItem> testdata{
      {0, false, false, false, false, ""},
      {1, false, false, false, true, "x"},
      {2, false, false, true, false, "w"},
      {3, false, false, true, true, "wx"},
      {4, false, true, false, false, "r"},
      {5, false, true, false, true, "rx"},
      {6, false, true, true, false, "rw"},
      {7, false, true, true, true, "rwx"},
      {8, true, false, false, false, "S"},
      {9, true, false, false, true, "xs"},
      {10, true, false, true, false, "wS"},
      {11, true, false, true, true, "wxs"},
      {12, true, true, false, false, "rS"},
      {13, true, true, false, true, "rxs"},
      {14, true, true, true, false, "rwS"},
      {15, true, true, true, true, "rwxs"},
  };

  for (const auto& row : testdata) {
    file::UserPerm userperm(row.bits);
    EXPECT_EQ(row.has_s, userperm.setxid());
    EXPECT_EQ(row.has_r, userperm.read());
    EXPECT_EQ(row.has_w, userperm.write());
    EXPECT_EQ(row.has_x, userperm.exec());
    EXPECT_EQ(row.str, userperm.as_string());
  }
}

TEST(Perm, Basics) {
  struct TestItem {
    uint16_t bits;

    bool has_us;
    bool has_gs;
    bool has_t;

    bool has_ur;
    bool has_uw;
    bool has_ux;

    bool has_gr;
    bool has_gw;
    bool has_gx;

    bool has_or;
    bool has_ow;
    bool has_ox;

    std::string str;
  };

  constexpr bool F = false;
  constexpr bool T = true;

  std::vector<TestItem> testdata{
      {04000, T, F, F, F, F, F, F, F, F, F, F, F, "04000"},
      {02000, F, T, F, F, F, F, F, F, F, F, F, F, "02000"},
      {01000, F, F, T, F, F, F, F, F, F, F, F, F, "01000"},
      {00400, F, F, F, T, F, F, F, F, F, F, F, F, "0400"},
      {00200, F, F, F, F, T, F, F, F, F, F, F, F, "0200"},
      {00100, F, F, F, F, F, T, F, F, F, F, F, F, "0100"},
      {00040, F, F, F, F, F, F, T, F, F, F, F, F, "0040"},
      {00020, F, F, F, F, F, F, F, T, F, F, F, F, "0020"},
      {00010, F, F, F, F, F, F, F, F, T, F, F, F, "0010"},
      {00004, F, F, F, F, F, F, F, F, F, T, F, F, "0004"},
      {00002, F, F, F, F, F, F, F, F, F, F, T, F, "0002"},
      {00001, F, F, F, F, F, F, F, F, F, F, F, T, "0001"},
      {00000, F, F, F, F, F, F, F, F, F, F, F, F, "0000"},
      {00751, F, F, F, T, T, T, T, F, T, F, F, T, "0751"},
      {04751, T, F, F, T, T, T, T, F, T, F, F, T, "04751"},
      {04640, T, F, F, T, T, F, T, F, F, F, F, F, "04640"},
  };

  for (const auto& row : testdata) {
    file::Perm p(row.bits);
    EXPECT_EQ(row.has_us, p.setuid());
    EXPECT_EQ(row.has_gs, p.setgid());
    EXPECT_EQ(row.has_t, p.sticky());
    EXPECT_EQ(row.has_ur, p.user_read());
    EXPECT_EQ(row.has_uw, p.user_write());
    EXPECT_EQ(row.has_ux, p.user_exec());
    EXPECT_EQ(row.has_gr, p.group_read());
    EXPECT_EQ(row.has_gw, p.group_write());
    EXPECT_EQ(row.has_gx, p.group_exec());
    EXPECT_EQ(row.has_or, p.other_read());
    EXPECT_EQ(row.has_ow, p.other_write());
    EXPECT_EQ(row.has_ox, p.other_exec());
    EXPECT_EQ(row.str, p.as_string());
  }

  file::Perm p;
  EXPECT_FALSE(p);
  EXPECT_EQ(0, uint16_t(p));
  EXPECT_FALSE(p.setxid());
  EXPECT_FALSE(p.read());
  EXPECT_FALSE(p.write());
  EXPECT_FALSE(p.exec());

  p = 0751;
  EXPECT_TRUE(p);
  EXPECT_EQ(0751, uint16_t(p));
  EXPECT_FALSE(p.setxid());
  EXPECT_TRUE(p.read());
  EXPECT_TRUE(p.write());
  EXPECT_TRUE(p.exec());

  p |= 04000;
  EXPECT_TRUE(p);
  EXPECT_EQ(04751, uint16_t(p));
  EXPECT_TRUE(p.setxid());
  EXPECT_TRUE(p.read());
  EXPECT_TRUE(p.write());
  EXPECT_TRUE(p.exec());

  p &= ~0111;
  EXPECT_TRUE(p);
  EXPECT_EQ(04640, uint16_t(p));
  EXPECT_TRUE(p.setxid());
  EXPECT_TRUE(p.read());
  EXPECT_TRUE(p.write());
  EXPECT_FALSE(p.exec());

  p &= ~0222;
  EXPECT_TRUE(p);
  EXPECT_EQ(04440, uint16_t(p));
  EXPECT_TRUE(p.setxid());
  EXPECT_TRUE(p.read());
  EXPECT_FALSE(p.write());
  EXPECT_FALSE(p.exec());

  p |= 01000;
  p &= ~06000;
  EXPECT_EQ(01440, uint16_t(p));
  EXPECT_FALSE(p.setxid());
  EXPECT_TRUE(p.read());
  EXPECT_FALSE(p.write());
  EXPECT_FALSE(p.exec());
}

static testing::AssertionResult PermConvertCheck(const char* expr,
                                                 file::Perm p) {
  if (p.setuid() != p.user().setxid())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .setuid() vs .user().setxid()";
  if (p.user_read() != p.user().read())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .user_read() vs .user().read()";
  if (p.user_write() != p.user().write())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .user_write() vs .user().write()";
  if (p.user_exec() != p.user().exec())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .user_exec() vs .user().exec()";

  if (p.setgid() != p.group().setxid())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .setgid() vs .group().setxid()";
  if (p.group_read() != p.group().read())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .group_read() vs .group().read()";
  if (p.group_write() != p.group().write())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .group_write() vs .group().write()";
  if (p.group_exec() != p.group().exec())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .group_exec() vs .group().exec()";

  if (p.other().setxid())
    return testing::AssertionFailure() << expr << " (" << p << ") "
                                       << "has discrepancy: .other().setxid()";
  if (p.other_read() != p.other().read())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .other_read() vs .other().read()";
  if (p.other_write() != p.other().write())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .other_write() vs .other().write()";
  if (p.other_exec() != p.other().exec())
    return testing::AssertionFailure()
           << expr << " (" << p << ") "
           << "has discrepancy: .other_exec() vs .other().exec()";

  return testing::AssertionSuccess();
}

#define EXPECT_PERM_CONVERT_OK(x) EXPECT_PRED_FORMAT1(PermConvertCheck, x)

TEST(Perm, Convert) {
  EXPECT_PERM_CONVERT_OK(file::Perm(04000));
  EXPECT_PERM_CONVERT_OK(file::Perm(02000));
  EXPECT_PERM_CONVERT_OK(file::Perm(01000));
  EXPECT_PERM_CONVERT_OK(file::Perm(00400));
  EXPECT_PERM_CONVERT_OK(file::Perm(00200));
  EXPECT_PERM_CONVERT_OK(file::Perm(00100));
  EXPECT_PERM_CONVERT_OK(file::Perm(00040));
  EXPECT_PERM_CONVERT_OK(file::Perm(00020));
  EXPECT_PERM_CONVERT_OK(file::Perm(00010));
  EXPECT_PERM_CONVERT_OK(file::Perm(00004));
  EXPECT_PERM_CONVERT_OK(file::Perm(00002));
  EXPECT_PERM_CONVERT_OK(file::Perm(00001));
}
