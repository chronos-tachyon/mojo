// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <iostream>

#include "base/result_testing.h"
#include "event/task.h"

static const char* const kStateNames[] = {
    "ready", "running", "cancelling", "finishing", "done", "cancelled",
};

static std::ostream& operator<<(std::ostream& os, event::Task::State s) {
  return (os << kStateNames[static_cast<uint8_t>(s)]);
}

TEST(Task, Inline) {
  int n = 0;
  auto inc = [&n] {
    ++n;
    return base::Result();
  };

  std::cerr << "1. create task" << std::endl;
  event::Task task;
  task.on_finished(event::callback(inc));
  EXPECT_EQ(event::Task::State::ready, task.state());
  EXPECT_FALSE(task.is_finished());
  EXPECT_EQ(0, n);

  std::cerr << "1. start task" << std::endl;
  EXPECT_TRUE(task.start());
  EXPECT_EQ(event::Task::State::running, task.state());
  EXPECT_FALSE(task.is_finished());
  EXPECT_EQ(0, n);

  std::cerr << "1. finish task [OK]" << std::endl;
  EXPECT_TRUE(task.finish_ok());
  EXPECT_EQ(event::Task::State::done, task.state());
  EXPECT_TRUE(task.is_finished());
  EXPECT_OK(task.result());
  EXPECT_EQ(1, n);

  std::cerr << "1. on_finished after finish" << std::endl;
  task.on_finished(event::callback(inc));
  EXPECT_EQ(2, n);

  std::cerr << "2. reset task" << std::endl;
  n = 0;
  task.reset();
  task.on_finished(event::callback(inc));
  EXPECT_EQ(event::Task::State::ready, task.state());
  EXPECT_EQ(0, n);

  std::cerr << "2. cancel task" << std::endl;
  EXPECT_TRUE(task.cancel());
  EXPECT_EQ(event::Task::State::cancelled, task.state());
  EXPECT_FALSE(task.is_cancelling());
  EXPECT_TRUE(task.is_finished());
  EXPECT_CANCELLED(task.result());
  EXPECT_EQ(1, n);

  std::cerr << "3. reset task" << std::endl;
  n = 0;
  task.reset();
  task.on_finished(event::callback(inc));
  EXPECT_EQ(event::Task::State::ready, task.state());
  EXPECT_EQ(0, n);

  std::cerr << "3. start task" << std::endl;
  EXPECT_TRUE(task.start());

  std::cerr << "3. cancel task" << std::endl;
  EXPECT_FALSE(task.cancel());
  EXPECT_EQ(event::Task::State::cancelling, task.state());
  EXPECT_TRUE(task.is_cancelling());
  EXPECT_FALSE(task.is_finished());
  EXPECT_EQ(0, n);

  std::cerr << "3. finish task [CANCELLED]" << std::endl;
  EXPECT_TRUE(task.finish_cancel());
  EXPECT_EQ(event::Task::State::cancelled, task.state());
  EXPECT_TRUE(task.is_finished());
  EXPECT_CANCELLED(task.result());
  EXPECT_EQ(1, n);
}

TEST(Task, Subtask) {
  event::Task parent;
  EXPECT_TRUE(parent.start());

  event::Task child0, child1;
  parent.add_subtask(&child0);
  parent.add_subtask(&child1);
  EXPECT_TRUE(child0.start());
  EXPECT_TRUE(child1.start());

  child0.finish_ok();
  EXPECT_FALSE(parent.cancel());

  EXPECT_EQ(event::Task::State::cancelling, parent.state());
  EXPECT_EQ(event::Task::State::done, child0.state());
  EXPECT_EQ(event::Task::State::cancelling, child1.state());

  child1.finish_cancel();
  parent.finish_cancel();

  EXPECT_EQ(event::Task::State::cancelled, parent.state());
  EXPECT_EQ(event::Task::State::done, child0.state());
  EXPECT_EQ(event::Task::State::cancelled, child1.state());
}
