// base/backport.h - Backports of C++14 and/or C++17 features
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_BACKPORT_H
#define BASE_BACKPORT_H

#include <cstdint>
#include <memory>

namespace base {
namespace backport {

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename T, T... Indices>
struct integer_sequence {
  using value_type = T;
  static constexpr std::size_t size() noexcept { return sizeof...(Indices); }
};

template <typename T, typename S1, typename S2>
struct integer_concat;

template <typename T, T... X, T... Y>
struct integer_concat<T, integer_sequence<T, X...>, integer_sequence<T, Y...>> {
  using type = integer_sequence<T, X..., (sizeof...(X) + Y)...>;
};

template <typename T, std::size_t N>
struct make_integer_sequence_helper {
  using left = typename make_integer_sequence_helper<T, N / 2>::type;
  using right = typename make_integer_sequence_helper<T, N - N / 2>::type;
  using type = typename integer_concat<T, left, right>::type;
};

template <typename T>
struct make_integer_sequence_helper<T, 0> {
  using type = integer_sequence<T>;
};

template <typename T>
struct make_integer_sequence_helper<T, 1> {
  using type = integer_sequence<T, 0>;
};

template <typename T>
struct make_integer_sequence_helper<T, 2> {
  using type = integer_sequence<T, 0, 1>;
};

template <typename T, T N>
using make_integer_sequence = typename make_integer_sequence_helper<T, N>::type;

template <std::size_t... Indices>
using index_sequence = integer_sequence<std::size_t, Indices...>;

template <std::size_t N>
using make_index_sequence = make_integer_sequence<std::size_t, N>;

}  // namespace backport
}  // namespace base

#endif  // BASE_BACKPORT_H
