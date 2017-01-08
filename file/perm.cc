// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/perm.h"

#include <cstring>

namespace file {

void UserPerm::append_to(std::string* out) const {
  if (read()) out->push_back('r');
  if (write()) out->push_back('w');
  if (exec()) out->push_back('x');
  if (setxid()) out->push_back(exec() ? 's' : 'S');
}

std::string UserPerm::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

void Perm::append_to(std::string* out) const {
  char buf[8];
  ::bzero(buf, sizeof(buf));

  char* begin = buf;
  char* ptr = begin;
  uint16_t n = bits_;
  while (n != 0) {
    *ptr = static_cast<unsigned char>(n & 7) + '0';
    ++ptr;
    n >>= 3;
  }
  while (ptr < (begin + 3)) {
    *ptr = '0';
    ++ptr;
  }
  *ptr = '0';
  ++ptr;
  while (ptr != begin) {
    --ptr;
    out->push_back(*ptr);
  }
}

std::string Perm::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

}  // namespace file
