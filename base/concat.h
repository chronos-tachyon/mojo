// base/concat.h - Concatenate strings and stringable objects
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_CONCAT_H
#define BASE_CONCAT_H

#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/backport.h"

namespace base {

// append_to stringifies |arg| and appends it to |out|.
//
// Typical usage:
//    std::string out;
//    using base::append_to;
//    append_to(out, obj);
//
template <typename T>
void append_to(std::string& out, T arg);

// length_hint guesses the length of |arg|'s stringified representation.
//
// This helps to reduce the asymptotic running time of calling append_to
// multiple times, reducing O(n log n) [due to reallocations] to O(n).
//
// Typical usage:
//    using base::length_hint;
//    std::size_t hint = length_hint(obj1) + length_hint(obj2);
//    std::string out;
//    out.reserve(hint);
//    using base::append_to;
//    append_to(out, obj1);
//    append_to(out, obj2);
//
template <typename T>
std::size_t length_hint(T arg);

namespace internal {

// Helper for breaking apart tuples {{{

template <typename... Types>
struct tuple_helper;

template <typename Only>
struct tuple_helper<Only> {
  using indices = backport::make_index_sequence<1U>;
  using type = std::tuple<Only>;

  using tail_indices = backport::make_index_sequence<0U>;
  using tail_type = std::tuple<>;

  static constexpr const Only& head(type t) { return std::get<0>(t); }

  static constexpr tail_type tail(type t) { return std::make_tuple(); }
};

template <typename First, typename Second, typename... Rest>
struct tuple_helper<First, Second, Rest...> {
  using indices = backport::make_index_sequence<2U + sizeof...(Rest)>;
  using type = std::tuple<First, Second, Rest...>;

  using tail_indices = backport::make_index_sequence<1U + sizeof...(Rest)>;
  using tail_type = std::tuple<Second, Rest...>;

  static constexpr const First& head(type t) { return std::get<0>(t); }

  template <std::size_t... I>
  static constexpr tail_type tail_impl(backport::index_sequence<I...>, type t) {
    return std::make_tuple(std::get<1U + I>(t)...);
  }

  static constexpr tail_type tail(type t) {
    return tail_impl(tail_indices(), t);
  }
};

// }}}
// Type detection helpers {{{

template <typename T, template <typename...> class Template>
struct is_specialization_of : public std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template>
    : public std::true_type {};

// }}}
// Type properties {{{

template <bool IsTuple, bool IsContainer, bool HasAppendTo, bool HasLengthHint>
struct properties {
  static constexpr bool is_tuple = IsTuple;
  static constexpr bool is_container = IsContainer;
  static constexpr bool has_append_to = HasAppendTo;
  static constexpr bool has_length_hint = HasLengthHint;
};

template <typename T>
struct properties_for {
 private:
  static constexpr bool is_tuple = is_specialization_of<T, std::tuple>::value ||
                                   is_specialization_of<T, std::pair>::value;

  template <typename>
  static constexpr std::false_type check_container(...) { return {}; }
  template <typename U>
  static constexpr std::true_type check_container(typename U::value_type*) { return {}; }
  using container_type = decltype(check_container<T>(nullptr));
  // Specifically exclude std::string from the definition of "a container".
  // The simple_appender for it works fine.
  static constexpr bool is_string = std::is_same<T, std::string>::value;
  static constexpr bool is_container = container_type::value && !is_string;

  template <typename>
  static constexpr std::false_type check_append_to(...) { return {}; }
  template <typename U>
  static constexpr auto check_append_to(U*) -> typename std::is_void<decltype(
      std::declval<const U&>().append_to(std::declval<std::string&>()))>::type { return {}; }
  using append_to_type = decltype(check_append_to<T>(nullptr));
  static constexpr bool has_append_to = append_to_type::value;

  template <typename>
  static constexpr std::false_type check_length_hint(...) { return {}; }
  template <typename U>
  static constexpr auto check_length_hint(U*) -> typename std::is_same<
      std::size_t, decltype(std::declval<const U&>().length_hint())>::type { return {}; }
  using length_hint_type = decltype(check_length_hint<T>(nullptr));
  static constexpr bool has_length_hint = length_hint_type::value;

 public:
  using type =
      properties<is_tuple, is_container, has_append_to, has_length_hint>;
};

// }}}
// Appenders for simple types {{{

template <typename T>
struct simple_appender;

inline void string_append_to(std::string& out, const std::string& arg) {
  out.append(arg);
}
inline std::size_t string_length_hint(const std::string& arg) {
  return arg.size();
}
template <>
struct simple_appender<std::string> {
  static void append_to(std::string& out, const std::string& arg) {
    string_append_to(out, arg);
  }
  static std::size_t length_hint(const std::string& arg) {
    return string_length_hint(arg);
  }
};

template <std::size_t N>
inline void chararray_append_to(std::string& out, const char (&arg)[N]) {
  out.append(arg, N - 1);
}
template <std::size_t N>
inline std::size_t chararray_length_hint(const char (&arg)[N]) {
  return N - 1;
}
template <>
struct simple_appender<char[]> {
  template <std::size_t N>
  static void append_to(std::string& out, const char (&arg)[N]) {
    chararray_append_to<N>(out, arg);
  }
  template <std::size_t N>
  static std::size_t length_hint(const char (&arg)[N]) {
    return chararray_length_hint<N>(arg);
  }
};

inline void charptr_append_to(std::string& out, const char* arg) {
  out.append(arg);
}
inline std::size_t charptr_length_hint(const char* arg) {
  return ::strlen(arg);
}
template <>
struct simple_appender<const char*> {
  static void append_to(std::string& out, const char* arg) {
    charptr_append_to(out, arg);
  }
  static std::size_t length_hint(const char* arg) {
    return charptr_length_hint(arg);
  }
};

void bool_append_to(std::string& out, bool arg);
inline std::size_t bool_length_hint(bool arg) { return 5; }
template <>
struct simple_appender<bool> {
  static void append_to(std::string& out, bool arg) {
    bool_append_to(out, arg);
  }
  static std::size_t length_hint(bool arg) { return bool_length_hint(arg); }
};

inline void char_append_to(std::string& out, char arg) { out.push_back(arg); }
inline std::size_t char_length_hint(char arg) { return 1; }
template <>
struct simple_appender<char> {
  static void append_to(std::string& out, char arg) {
    char_append_to(out, arg);
  }
  static std::size_t length_hint(char arg) { return char_length_hint(arg); }
};

void sc_append_to(std::string& out, signed char arg);
inline std::size_t sc_length_hint(char arg) {
  return std::numeric_limits<signed char>::digits10;
}
template <>
struct simple_appender<signed char> {
  static void append_to(std::string& out, signed char arg) {
    sc_append_to(out, arg);
  }
  static std::size_t length_hint(signed char arg) {
    return sc_length_hint(arg);
  }
};

void ss_append_to(std::string& out, signed short arg);
inline std::size_t ss_length_hint(short arg) {
  return std::numeric_limits<signed short>::digits10;
}
template <>
struct simple_appender<signed short> {
  static void append_to(std::string& out, signed short arg) {
    ss_append_to(out, arg);
  }
  static std::size_t length_hint(signed short arg) {
    return ss_length_hint(arg);
  }
};

void si_append_to(std::string& out, signed int arg);
inline std::size_t si_length_hint(int arg) {
  return std::numeric_limits<signed int>::digits10;
}
template <>
struct simple_appender<signed int> {
  static void append_to(std::string& out, signed int arg) {
    si_append_to(out, arg);
  }
  static std::size_t length_hint(signed int arg) { return si_length_hint(arg); }
};

void sl_append_to(std::string& out, signed long arg);
inline std::size_t sl_length_hint(long arg) {
  return std::numeric_limits<signed long>::digits10;
}
template <>
struct simple_appender<signed long> {
  static void append_to(std::string& out, signed long arg) {
    sl_append_to(out, arg);
  }
  static std::size_t length_hint(signed long arg) {
    return sl_length_hint(arg);
  }
};

void sll_append_to(std::string& out, signed long long arg);
inline std::size_t sll_length_hint(long long arg) {
  return std::numeric_limits<signed long long>::digits10;
}
template <>
struct simple_appender<signed long long> {
  static void append_to(std::string& out, signed long long arg) {
    sll_append_to(out, arg);
  }
  static std::size_t length_hint(signed long long arg) {
    return sll_length_hint(arg);
  }
};

void uc_append_to(std::string& out, unsigned char arg);
inline std::size_t uc_length_hint(char arg) {
  return std::numeric_limits<unsigned char>::digits10;
}
template <>
struct simple_appender<unsigned char> {
  static void append_to(std::string& out, unsigned char arg) {
    uc_append_to(out, arg);
  }
  static std::size_t length_hint(unsigned char arg) {
    return uc_length_hint(arg);
  }
};

void us_append_to(std::string& out, unsigned short arg);
inline std::size_t us_length_hint(short arg) {
  return std::numeric_limits<unsigned short>::digits10;
}
template <>
struct simple_appender<unsigned short> {
  static void append_to(std::string& out, unsigned short arg) {
    us_append_to(out, arg);
  }
  static std::size_t length_hint(unsigned short arg) {
    return us_length_hint(arg);
  }
};

void ui_append_to(std::string& out, unsigned int arg);
inline std::size_t ui_length_hint(int arg) {
  return std::numeric_limits<unsigned int>::digits10;
}
template <>
struct simple_appender<unsigned int> {
  static void append_to(std::string& out, unsigned int arg) {
    ui_append_to(out, arg);
  }
  static std::size_t length_hint(unsigned int arg) {
    return ui_length_hint(arg);
  }
};

void ul_append_to(std::string& out, unsigned long arg);
inline std::size_t ul_length_hint(long arg) {
  return std::numeric_limits<unsigned long>::digits10;
}
template <>
struct simple_appender<unsigned long> {
  static void append_to(std::string& out, unsigned long arg) {
    ul_append_to(out, arg);
  }
  static std::size_t length_hint(unsigned long arg) {
    return ul_length_hint(arg);
  }
};

void ull_append_to(std::string& out, unsigned long long arg);
inline std::size_t ull_length_hint(long long arg) {
  return std::numeric_limits<unsigned long long>::digits10;
}
template <>
struct simple_appender<unsigned long long> {
  static void append_to(std::string& out, unsigned long long arg) {
    ull_append_to(out, arg);
  }
  static std::size_t length_hint(unsigned long long arg) {
    return ull_length_hint(arg);
  }
};

// TODO: simple_appender<float>
// TODO: simple_appender<double>
// TODO: simple_appender<long double>

// }}}
// Appenders for advanced types {{{

template <typename T, typename P>
struct advanced_appender;

// Fallthrough for simple types {{{

template <typename T>
struct advanced_appender<T, properties<false, false, false, false>>
    : public simple_appender<T> {};

template <typename T>
struct advanced_appender<T, properties<false, false, false, true>>
    : public simple_appender<T> {};

// }}}
// Objects with append_to method (w/ or w/o length_hint method) {{{

template <typename T>
struct advanced_appender<T, properties<false, false, true, false>> {
  static void append_to(std::string& out, const T& arg) { arg.append_to(out); }
  static std::size_t length_hint(const T& arg) { return 0; }
};

template <typename T>
struct advanced_appender<T, properties<false, false, true, true>> {
  static void append_to(std::string& out, const T& arg) { arg.append_to(out); }
  static std::size_t length_hint(const T& arg) { return arg.length_hint(); }
};

// }}}
// Container types {{{

template <typename T>
struct advanced_appender<T, properties<false, true, false, false>> {
  static void append_to(std::string& out, const T& arg) {
    using base::append_to;
    out.push_back('[');
    for (const auto& item : arg) {
      append_to(out, item);
      out.append(", ", 2);
    }
    if (!arg.empty()) out.resize(out.size() - 2);
    out.push_back(']');
  }
  static std::size_t length_hint(const T& arg) {
    using base::length_hint;
    if (arg.empty()) return 2;
    std::size_t n = 2 * arg.size() + 1;
    for (const auto& item : arg) {
      n += length_hint(item);
    }
    return n;
  }
};

template <typename T>
struct advanced_appender<T, properties<false, true, false, true>>
    : public advanced_appender<T, properties<false, true, false, false>> {};

// }}}
// Container types with append_to method (append_to wins) {{{

template <typename T>
struct advanced_appender<T, properties<false, true, true, false>>
    : public advanced_appender<T, properties<false, false, true, false>> {};

template <typename T>
struct advanced_appender<T, properties<false, true, true, true>>
    : public advanced_appender<T, properties<false, false, true, true>> {};

// }}}
// Tuple and pair types {{{

template <typename T>
struct advanced_appender<T, properties<true, false, false, false>> {
  static void partial_append_to(std::string& out, std::tuple<> t) {}

  static std::size_t partial_length_hint(std::tuple<> t) { return 0; }

  template <typename First, typename... Rest>
  static void partial_append_to(std::string& out,
                                std::tuple<First, Rest...> t) {
    using H = tuple_helper<First, Rest...>;
    using base::append_to;
    append_to(out, H::head(t));
    out.append(", ", 2);
    partial_append_to(out, H::tail(t));
  }

  template <typename First, typename... Rest>
  static std::size_t partial_length_hint(std::tuple<First, Rest...> t) {
    using H = tuple_helper<First, Rest...>;
    using base::length_hint;
    return 2 + length_hint(H::head(t)) + partial_length_hint(H::tail(t));
  }

  template <typename... Types>
  static void append_to(std::string& out, std::tuple<Types...> t) {
    out.push_back('<');
    partial_append_to(out, t);
    if (std::tuple_size<std::tuple<Types...>>::value != 0)
      out.resize(out.size() - 2);
    out.push_back('>');
  }

  template <typename... Types>
  static std::size_t length_hint(std::tuple<Types...> t) {
    using base::length_hint;
    return 2 + partial_length_hint(t);
  }

  template <typename U, typename V>
  static void append_to(std::string& out, std::pair<U, V> p) {
    return append_to(out, std::make_tuple(p.first, p.second));
  }

  template <typename U, typename V>
  static std::size_t length_hint(std::pair<U, V> p) {
    return length_hint(std::make_tuple(p.first, p.second));
  }
};

template <typename T>
struct advanced_appender<T, properties<true, false, false, true>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

// }}}
// Tuples/Pairs with append_to method (Can't Happen™) {{{

template <typename T>
struct advanced_appender<T, properties<true, false, true, false>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

template <typename T>
struct advanced_appender<T, properties<true, false, true, true>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

// }}}
// Tuples/Pairs that are also container types (Can't Happen™) {{{

template <typename T>
struct advanced_appender<T, properties<true, true, false, false>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

template <typename T>
struct advanced_appender<T, properties<true, true, false, true>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

// }}}
// Tuples/Pairs that are also container types with append_to methods (WTF?) {{{

template <typename T>
struct advanced_appender<T, properties<true, true, true, false>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

template <typename T>
struct advanced_appender<T, properties<true, true, true, true>>
    : public advanced_appender<T, properties<true, false, false, false>> {};

// }}}
// }}}

}  // namespace internal

// The actual implementations for append_to and length_hint just thunk to the
// real implementations provided by advanced_appender.

template <typename T>
void append_to(std::string& out, T arg) {
  using JFT = typename std::remove_cv<typename std::remove_extent<
      typename std::remove_reference<T>::type>::type>::type;
  using P = typename internal::properties_for<JFT>::type;
  using A = internal::advanced_appender<JFT, P>;
  A::append_to(out, arg);
}

template <typename T>
std::size_t length_hint(T arg) {
  using JFT = typename std::remove_cv<typename std::remove_extent<
      typename std::remove_reference<T>::type>::type>::type;
  using P = typename internal::properties_for<JFT>::type;
  using A = internal::advanced_appender<JFT, P>;
  return A::length_hint(arg);
}

namespace internal {

// Internals for concat_to / concat {{{

inline void concat_to_impl(std::string& out) {}
inline std::size_t concat_length() { return 0; }

template <typename T>
void concat_to_impl(std::string& out, const T& arg) {
  using base::append_to;
  append_to(out, arg);
}

template <typename T>
std::size_t concat_length(const T& arg) {
  using base::length_hint;
  return length_hint(arg);
}

template <typename T, typename U, typename... Rest>
void concat_to_impl(std::string& out, const T& arg1, const U& arg2,
                    const Rest&... rest) {
  using base::append_to;
  append_to(out, arg1);
  concat_to_impl(out, arg2, rest...);
}

template <typename T, typename U, typename... Rest>
std::size_t concat_length(const T& arg1, const U& arg2, const Rest&... rest) {
  using base::length_hint;
  return length_hint(arg1) + concat_length(arg2, rest...);
}

// }}}

}  // namespace internal

// concat_to appends zero or more object string representations to |out|.
//
// Typical usage:
//    std::string out;
//    base::concat_to(out, obj1);
//    if (cond) base::concat_to(out, " vs ", obj2);
//    base::concat_to(out, ", but don't forget ", obj3);
//
template <typename... Args>
void concat_to(std::string& out, const Args&... args) {
  out.reserve(out.size() + internal::concat_length(args...));
  internal::concat_to_impl(out, args...);
}

// concat concatenates zero or more object string representations.
//
// Typical usage:
//    std::string out = base::concat(obj1, " vs ", obj2);
//
template <typename... Args>
std::string concat(const Args&... args) {
  std::string out;
  concat_to(out, args...);
  return out;
}

}  // namespace base

#endif  // BASE_CONCAT_H
