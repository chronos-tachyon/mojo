// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/set.h"

#include "base/concat.h"

__attribute__((const)) static std::size_t popcount(unsigned int x) noexcept {
  return __builtin_popcount(x);
}

namespace event {

void Set::append_to(std::string* out) const {
  out->push_back('[');
  if (readable()) out->push_back('r');
  if (writable()) out->push_back('w');
  if (priority()) out->push_back('p');
  if (hangup()) out->push_back('h');
  if (error()) out->push_back('e');
  if (signal()) out->push_back('S');
  if (timer()) out->push_back('T');
  if (generic()) out->push_back('G');
  out->push_back(']');
}

std::size_t Set::length_hint() const noexcept { return 2 + popcount(bits_); }

std::string Set::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

}  // namespace event
