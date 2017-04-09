// base/strings.h - StringPiece and other string helpers
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_STRINGS_H
#define BASE_STRINGS_H

#include <climits>
#include <cstring>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/chars.h"
#include "base/concat.h"
#include "external/com_googlesource_code_re2/re2/stringpiece.h"

namespace base {

using is_exactly = base::charmatch::is_exactly;
using is_oneof = base::charmatch::is_oneof;
using is_whitespace = base::charmatch::is_whitespace;
using is_eol = base::charmatch::is_eol;

using StringPiece = Chars;

class SplitterImpl {
 protected:
  SplitterImpl() noexcept = default;

 public:
  virtual ~SplitterImpl() noexcept = default;

  // If |sp| can be split, splits it into |first| + |rest| and returns true.
  // Otherwise, copies |sp| to |first| and returns false.
  //
  // Example (splitting on ','):
  //    sp      | return  | first | rest
  //    --------+---------+-------+----------
  //    "a,b,c" | true    | "a"   | "b,c"
  //    "b,c"   | true    | "b"   | "c"
  //    "c"     | false   | "c"   | <ignored>
  virtual bool chop(StringPiece* first, StringPiece* rest,
                    StringPiece sp) const = 0;

  SplitterImpl(const SplitterImpl&) = delete;
  SplitterImpl(SplitterImpl&&) = delete;
  SplitterImpl& operator=(const SplitterImpl&) = delete;
  SplitterImpl& operator=(SplitterImpl&&) = delete;
};

class Splitter {
 public:
  using Pointer = std::shared_ptr<SplitterImpl>;
  using Predicate = std::function<bool(char)>;

  // Splitter is constructible from an implementation.
  Splitter(Pointer ptr) : ptr_(std::move(ptr)), lim_(SIZE_MAX), omit_(false) {}

  // Splitter is default constructible.
  Splitter() : Splitter(nullptr) {}

  // Splitter is copyable and moveable.
  Splitter(const Splitter&) = default;
  Splitter(Splitter&&) = default;
  Splitter& operator=(const Splitter&) = default;
  Splitter& operator=(Splitter&&) = default;

  // Returns true iff this Splitter is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Splitter is non-empty.
  void assert_valid() const noexcept;

  // Returns this Splitter's implementation.
  const Pointer& implementation() const noexcept { return ptr_; }
  Pointer& implementation() noexcept { return ptr_; }

  // Trims all characters matching |pred|
  // from the beginning and end of each item.
  Splitter& trim(Predicate pred) {
    trim_ = std::move(pred);
    return *this;
  }

  // Trims |ch| from the beginning and end of each item.
  Splitter& trim(char ch) {
    trim_ = is_exactly(ch);
    return *this;
  }

  // Trims whitespace from the beginning and end of each item.
  Splitter& trim_whitespace() {
    trim_ = is_whitespace();
    return *this;
  }

  // Limits the output to |n| items.
  // - 0 is impossible; it is treated as 1
  Splitter& limit(std::size_t n) noexcept {
    lim_ = n;
    return *this;
  }

  // Removes any limit on the number of items to be output.
  Splitter& unlimited() noexcept {
    lim_ = SIZE_MAX;
    return *this;
  }

  // Make this Splitter omit empty items.
  //
  // Example (splitting on ','):
  //                Input: "a,,b,c"
  //    Output (standard): {"a", "", "b", "c"}
  //  Output (omit_empty): {"a", "b", "c"}
  //
  Splitter& omit_empty(bool value = true) noexcept {
    omit_ = value;
    return *this;
  }

  // Splits |sp| into pieces.
  std::vector<StringPiece> split(StringPiece sp) const;

  // Splits |sp| into pieces.  The pieces are returned as std::strings.
  std::vector<std::string> split_strings(StringPiece sp) const;

 private:
  Pointer ptr_;
  Predicate trim_;
  std::size_t lim_;
  bool omit_;
};

class JoinerImpl {
 protected:
  JoinerImpl() noexcept = default;

 public:
  virtual ~JoinerImpl() noexcept = default;

  virtual void glue(std::string* out, StringPiece sp, bool first) const = 0;
  virtual std::size_t hint() const noexcept = 0;

  JoinerImpl(const JoinerImpl&) = delete;
  JoinerImpl(JoinerImpl&&) = delete;
  JoinerImpl& operator=(const JoinerImpl&) = delete;
  JoinerImpl& operator=(JoinerImpl&&) = delete;
};

class Join {
 public:
  using Pointer = std::shared_ptr<JoinerImpl>;
  using Vector = std::vector<std::string>;

  Join(const Join&) = default;
  Join(Join&&) noexcept = default;
  Join& operator=(const Join&) = default;
  Join& operator=(Join&&) noexcept = default;

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;

  operator std::string() const {
    std::string out;
    append_to(&out);
    return out;
  }

 private:
  friend class Joiner;

  Join(Vector v, Pointer p, bool s) noexcept : vec_(std::move(v)),
                                               ptr_(std::move(p)),
                                               skip_(s) {}

  Vector vec_;
  Pointer ptr_;
  bool skip_;
};

class Joiner {
 public:
  using Pointer = std::shared_ptr<JoinerImpl>;

  // Joiner is constructible from an implementation.
  Joiner(Pointer ptr) : ptr_(std::move(ptr)), skip_(false) {}

  // Joiner is default constructible.
  Joiner() : Joiner(nullptr) {}

  // Joiner is copyable and moveable.
  Joiner(const Joiner&) = default;
  Joiner(Joiner&&) = default;
  Joiner& operator=(const Joiner&) = default;
  Joiner& operator=(Joiner&&) = default;

  // Returns true iff this Joiner is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Joiner is non-empty.
  void assert_valid() const noexcept;

  // Returns this Joiner's implementation.
  const Pointer& implementation() const noexcept { return ptr_; }
  Pointer& implementation() noexcept { return ptr_; }

  // Make this Joiner skip empty items.
  //
  // Example (joining on ','):
  //                Input: {"a", "", "b", "c"}
  //    Output (standard): "a,,b,c"
  //  Output (omit_empty): "a,b,c"
  //
  Joiner& skip_empty(bool value = true) noexcept {
    skip_ = value;
    return *this;
  }

 private:
  static void build_vector(std::vector<std::string>* out) {}

  template <typename T>
  static void build_vector(std::vector<std::string>* out, T&& arg) {
    std::string tmp;
    using base::append_to;
    append_to(&tmp, std::forward<T>(arg));
    out->push_back(std::move(tmp));
  }

  template <typename First, typename Second, typename... Rest>
  static void build_vector(std::vector<std::string>* out, First&& first,
                           Second&& second, Rest&&... rest) {
    build_vector(out, std::forward<First>(first));
    build_vector(out, std::forward<Second>(second),
                 std::forward<Rest>(rest)...);
  }

 public:
  Join join(const std::vector<StringPiece>& vec) const;
  Join join(const std::vector<std::string>& vec) const;

  template <typename... Args>
  Join join(Args&&... args) const {
    assert_valid();
    std::vector<StringPiece> vec;
    vec.resize(sizeof...(args));
    build_vector(&vec, std::forward<Args>(args)...);
    return Join(vec, ptr_, skip_);
  }

  void join_append(std::string* out,
                   const std::vector<StringPiece>& vec) const {
    join(vec).append_to(out);
  }
  void join_append(std::string* out,
                   const std::vector<std::string>& vec) const {
    join(vec).append_to(out);
  }
  template <typename... Args>
  void join_append(std::string* out, Args&&... args) const {
    join(std::forward<Args>(args)...).append_to(out);
  }

  std::string join_string(const std::vector<StringPiece>& vec) const {
    return join(vec);
  }
  std::string join_string(const std::vector<std::string>& vec) const {
    return join(vec);
  }
  template <typename... Args>
  std::string join_string(Args&&... args) const {
    return join(std::forward<Args>(args)...);
  }

 private:
  Pointer ptr_;
  bool skip_;
};

namespace split {

using Predicate = std::function<bool(char)>;

Splitter fixed_length(std::size_t len);
Splitter on(char ch);
Splitter on(std::string str);
Splitter on(Predicate pred);
Splitter on_pattern(StringPiece pattern);

}  // namespace split
namespace join {

Joiner on();
Joiner on(char ch);
Joiner on(std::string str);

}  // namespace join
}  // namespace base

#endif  // BASE_STRINGS_H
