// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/mode.h"

#include "base/logging.h"

namespace file {

Mode::Mode(const char* str) noexcept : bits_(0) {
  CHECK_NOTNULL(str);
  if (*str == 'r') {
    bits_ |= bit_r;
    ++str;
  }
  if (*str == 'w') {
    bits_ |= bit_w;
    ++str;
  }
  if (*str == 'a') {
    bits_ |= bit_a;
    ++str;
  }
  if (*str == 'c') {
    bits_ |= bit_c;
    ++str;
  }
  if (*str == 'x') {
    bits_ |= bit_x;
    ++str;
  }
  if (*str == 't') {
    bits_ |= bit_t;
    ++str;
  }
  CHECK_EQ("", std::string(str)) << ": trailing garbage";
}

bool Mode::valid() const noexcept {
  if (append() && !write()) return false;
  if (create() && !write()) return false;
  if (exclusive() && !create()) return false;
  if (truncate() && !write()) return false;
  return true;
}

void Mode::append_to(std::string* out) const {
  if (read()) out->push_back('r');
  if (write()) out->push_back('w');
  if (append()) out->push_back('a');
  if (create()) out->push_back('c');
  if (exclusive()) out->push_back('x');
  if (truncate()) out->push_back('t');
}

std::string Mode::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

}  // namespace file
