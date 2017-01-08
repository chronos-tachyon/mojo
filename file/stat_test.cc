// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "file/stat.h"

TEST(StatFS, AsString) {
  file::StatFS fst;
  EXPECT_EQ(
      "StatFS{optimal_block_size=0, used_blocks=0, free_blocks=0, "
      "used_inodes=0, free_inodes=0}",
      fst.as_string());
}

TEST(Stat, AsString) {
  file::Stat st;
  EXPECT_EQ(
      "Stat{type=unknown, perm=0000, owner=\"\", group=\"\", "
      "link_count=0, size=0, size_blocks=0, optimal_block_size=0}",
      st.as_string());

  st.type = file::FileType::regular;
  st.perm = 0755;
  st.owner = "root";
  st.group = "root";
  EXPECT_EQ(
      "Stat{type=regular, perm=0755, owner=\"root\", "
      "group=\"root\", link_count=0, size=0, size_blocks=0, "
      "optimal_block_size=0}",
      st.as_string());
}
