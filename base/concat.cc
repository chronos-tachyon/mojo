// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/concat.h"

#include <type_traits>

namespace {

template <typename T>
void convert_unsigned(std::string* out, T arg) {
  if (arg == 0) {
    out->push_back('0');
    return;
  }

  std::string tmp;
  while (arg != 0) {
    tmp.push_back(static_cast<unsigned char>(arg % 10) + '0');
    arg /= 10;
  }
  for (auto it = tmp.crbegin(), end = tmp.crend(); it != end; ++it) {
    out->push_back(*it);
  }
}

template <typename T>
void convert_signed(std::string* out, T arg) {
  if (arg < 0) {
    out->push_back('-');
    arg = -arg;
  }
  convert_unsigned(out, static_cast<typename std::make_unsigned<T>::type>(arg));
}

}  // anonymous namespace

namespace base {
namespace internal {

void bool_append_to(std::string* out, bool arg) {
  if (arg)
    chararray_append_to(out, "true");
  else
    chararray_append_to(out, "false");
}

void sc_append_to(std::string* out, signed char arg) {
  convert_signed(out, arg);
}
void ss_append_to(std::string* out, signed short arg) {
  convert_signed(out, arg);
}
void si_append_to(std::string* out, signed int arg) {
  convert_signed(out, arg);
}
void sl_append_to(std::string* out, signed long arg) {
  convert_signed(out, arg);
}
void sll_append_to(std::string* out, signed long long arg) {
  convert_signed(out, arg);
}

void uc_append_to(std::string* out, unsigned char arg) {
  convert_unsigned(out, arg);
}
void us_append_to(std::string* out, unsigned short arg) {
  convert_unsigned(out, arg);
}
void ui_append_to(std::string* out, unsigned int arg) {
  convert_unsigned(out, arg);
}
void ul_append_to(std::string* out, unsigned long arg) {
  convert_unsigned(out, arg);
}
void ull_append_to(std::string* out, unsigned long long arg) {
  convert_unsigned(out, arg);
}

}  // namespace internal
}  // namespace base
