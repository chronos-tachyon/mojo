#include "gtest/gtest.h"

#include "base/user.h"

TEST(User, Basics) {
  base::User empty;
  EXPECT_EQ(-1, empty.uid);
  EXPECT_EQ(-1, empty.gid);
  EXPECT_EQ("", empty.name);
  EXPECT_EQ("(-1)", empty.as_string());

  base::User u(23, 1000, "alice");
  EXPECT_EQ(23, u.uid);
  EXPECT_EQ(1000, u.gid);
  EXPECT_EQ("alice", u.name);
  EXPECT_EQ("alice(23)", u.as_string());

  u = base::User(42, 1000, "bob");
  EXPECT_EQ(42, u.uid);
  EXPECT_EQ(1000, u.gid);
  EXPECT_EQ("bob", u.name);
  EXPECT_EQ("bob(42)", u.as_string());

  u = base::User(17);
  EXPECT_EQ(17, u.uid);
  EXPECT_EQ(-1, u.gid);
  EXPECT_EQ("", u.name);
  EXPECT_EQ("(17)", u.as_string());
}

TEST(Group, Basics) {
  base::Group empty;
  EXPECT_EQ(-1, empty.gid);
  EXPECT_EQ("", empty.name);
  EXPECT_EQ("(-1)", empty.as_string());

  base::Group g(1000, "users");
  EXPECT_EQ(1000, g.gid);
  EXPECT_EQ("users", g.name);
  EXPECT_EQ("users(1000)", g.as_string());

  g = base::Group(1001, "staff");
  EXPECT_EQ(1001, g.gid);
  EXPECT_EQ("staff", g.name);
  EXPECT_EQ("staff(1001)", g.as_string());

  g = base::Group(1002);
  EXPECT_EQ(1002, g.gid);
  EXPECT_EQ("", g.name);
  EXPECT_EQ("(1002)", g.as_string());
}
