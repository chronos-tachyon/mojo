#include "gtest/gtest.h"

#include "base/logging.h"
#include "base/result_testing.h"
#include "io/pipe.h"

TEST(Pipe, EndToEnd) {
  io::Options o;
  io::Reader r;
  io::Writer w;
  io::make_pipe(&r, &w, o);

  event::Task rd0, rd1, rd2;
  std::size_t n0, n1, n2;
  char buf[16];
  ::bzero(buf, sizeof(buf));

  event::Task wr0, wr1, wr2;
  std::size_t m0, m1, m2;

  LOG(INFO) << "reading 8 bytes at offset 0";
  r.read(&rd0, buf, &n0, 8, 8);

  LOG(INFO) << "reading 4 bytes at offset 8";
  r.read(&rd1, buf + 8, &n1, 4, 4);

  LOG(INFO) << "writing 4 bytes";
  w.write(&wr0, &m0, "abcd", 4);

  LOG(INFO) << "writing 8 bytes";
  w.write(&wr1, &m1, "efghijkl", 8);

  LOG(INFO) << "writing 4 bytes";
  w.write(&wr2, &m2, "mnop", 4);

  LOG(INFO) << "reading 4 bytes at offset 12";
  r.read(&rd2, buf + 12, &n2, 4, 4);

  LOG(INFO) << "waiting for all tasks";
  event::wait_all({o.manager()}, {&rd0, &rd1, &rd2, &wr0, &wr1, &wr2});

  EXPECT_OK(wr0.result());
  EXPECT_OK(wr1.result());
  EXPECT_OK(wr2.result());
  EXPECT_EQ(4U, m0);
  EXPECT_EQ(8U, m1);
  EXPECT_EQ(4U, m2);
  EXPECT_OK(rd0.result());
  EXPECT_OK(rd1.result());
  EXPECT_OK(rd2.result());
  EXPECT_EQ(8U, n0);
  EXPECT_EQ(4U, n1);
  EXPECT_EQ(4U, n2);
  EXPECT_EQ("abcdefghijklmnop", std::string(buf, 16));

  event::Task rd3, wr3, cl;
  std::size_t n3, m3;

  LOG(INFO) << "writing 2 bytes";
  w.write(&wr3, &m3, "qr", 2);

  LOG(INFO) << "closing pipe";
  w.close(&cl);

  LOG(INFO) << "waiting for completion";
  event::wait_all({o.manager()}, {&wr3, &cl});

  EXPECT_OK(wr3.result());
  EXPECT_EQ(2U, m3);
  EXPECT_OK(cl.result());

  LOG(INFO) << "reading 4 bytes at offset 0";
  r.read(&rd3, buf, &n3, 4, 4);

  LOG(INFO) << "waiting for completion";
  event::wait(o.manager(), &rd3);

  EXPECT_EOF(rd3.result());
  EXPECT_EQ(2U, n3);
  EXPECT_EQ("qr", std::string(buf, 2));
}
