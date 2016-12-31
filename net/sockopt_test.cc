// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <sys/socket.h>
#include <sys/types.h>

#include "base/fd.h"
#include "base/result_testing.h"
#include "net/sockopt.h"

TEST(SockOpt, AsString) {
  EXPECT_EQ("<SOL_SOCKET, SO_ERROR>", net::sockopt_error.as_string());
  EXPECT_EQ("<IPPROTO_TCP, TCP_NODELAY>", net::sockopt_tcp_nodelay.as_string());
}

static base::Result get_int(base::FD fd, net::SockOpt opt, int* value) {
  unsigned int value_len = sizeof(*value);
  return opt.get(fd, value, &value_len);
}

static base::Result set_int(base::FD fd, net::SockOpt opt, int value) {
  return opt.set(fd, &value, sizeof(value));
}

TEST(SockOpt, GetSet) {
  base::SocketPair s;
  EXPECT_OK(base::make_socketpair(&s, AF_UNIX, SOCK_STREAM, 0));

  int val = -1;
  EXPECT_OK(get_int(s.left, net::sockopt_error, &val));
  EXPECT_EQ(0, val);

  val = -1;
  EXPECT_OK(set_int(s.left, net::sockopt_rcvbuf, 4096));
  EXPECT_OK(get_int(s.left, net::sockopt_rcvbuf, &val));
  EXPECT_GE(val, 4096);
}
