// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/strings.h"

#include <ostream>

#include "base/logging.h"
#include "external/com_googlesource_code_re2/re2/re2.h"

namespace base {

namespace {

class FixedSplitter : public SplitterImpl {
 public:
  explicit FixedSplitter(std::size_t len) noexcept : len_(len) {}

  bool chop(StringPiece* first, StringPiece* rest, StringPiece sp) const
      noexcept override {
    if (sp.size() > len_) {
      *first = sp.substring(0, len_);
      *rest = sp.substring(len_);
      return true;
    }
    *first = sp;
    return false;
  }

 private:
  std::size_t len_;
};

class CharSplitter : public SplitterImpl {
 public:
  explicit CharSplitter(char ch) noexcept : ch_(ch) {}

  bool chop(StringPiece* first, StringPiece* rest, StringPiece sp) const
      noexcept override {
    auto index = sp.find(ch_);
    if (index != StringPiece::npos) {
      *first = sp.substring(0, index);
      *rest = sp.substring(index + 1);
      return true;
    }
    *first = sp;
    return false;
  }

 private:
  char ch_;
};

class StringSplitter : public SplitterImpl {
 public:
  explicit StringSplitter(std::string str) noexcept : str_(std::move(str)) {}

  bool chop(StringPiece* first, StringPiece* rest, StringPiece sp) const
      noexcept override {
    if (str_.empty()) {
      if (!sp.empty()) {
        *first = sp.substring(0, 1);
        *rest = sp.substring(1);
        return true;
      }
    } else {
      auto index = sp.find(str_);
      LOG(INFO) << "sp=[" << sp << "], "
                << "str=[" << str_ << "], "
                << "index=" << index;
      if (index != StringPiece::npos) {
        *first = sp.substring(0, index);
        *rest = sp.substring(index + str_.size());
        return true;
      }
    }
    *first = sp;
    return false;
  }

 private:
  std::string str_;
};

class PredicateSplitter : public SplitterImpl {
 public:
  using Predicate = split::Predicate;

  explicit PredicateSplitter(Predicate pred) noexcept : pred_(pred) {}

  bool chop(StringPiece* first, StringPiece* rest, StringPiece sp) const
      noexcept override {
    auto begin = sp.begin(), end = sp.end();
    auto it = begin;
    while (it != end) {
      if (pred_(*it)) {
        *first = sp.substring(0, it - begin);
        *rest = sp.substring(it - begin + 1);
        return true;
      }
      ++it;
    }
    *first = sp;
    return false;
  }

 private:
  Predicate pred_;
};

class PatternSplitter : public SplitterImpl {
 public:
  explicit PatternSplitter(StringPiece pattern) noexcept
      : re_(pattern.as_string()) {
    CHECK(re_.ok()) << ": " << re_.error();
  }

  bool chop(StringPiece* first, StringPiece* rest, StringPiece sp) const
      noexcept override {
    re2::StringPiece input = sp;
    re2::StringPiece match;
    if (re_.Match(input, 0, input.size(), re2::RE2::UNANCHORED, &match, 1)) {
      std::size_t index = match.data() - input.data();
      *first = sp.substring(0, index);
      *rest = sp.substring(index + match.size());
      return true;
    }
    *first = sp;
    return false;
  }

 private:
  re2::RE2 re_;
};

}  // anonymous namespace

constexpr std::size_t StringPiece::npos;

void StringPiece::append_to(std::string* out) const {
  out->append(data_, size_);
}

std::ostream& operator<<(std::ostream& o, StringPiece sp) {
  return (o << sp.as_string());
}

void Splitter::assert_valid() const noexcept {
  if (ptr_) return;
  LOG(FATAL) << "BUG! base::split::Splitter is empty";
}

std::vector<StringPiece> Splitter::split(StringPiece sp) {
  assert_valid();
  std::vector<StringPiece> out;
  StringPiece first, rest;
  std::size_t n = 0;
  bool more = true;
  while (more) {
    ++n;
    if (n >= lim_) {
      out.push_back(sp);
      break;
    }
    more = ptr_->chop(&first, &rest, sp);
    if (trim_) {
      while (!first.empty() && trim_(first.front())) first.remove_prefix(1);
      while (!first.empty() && trim_(first.back())) first.remove_suffix(1);
    }
    sp = rest;
    if (omit_ && first.empty()) {
      --n;
    } else {
      out.push_back(first);
    }
  }
  return out;
}

std::vector<std::string> Splitter::split_strings(StringPiece sp) {
  auto in = split(sp);
  std::vector<std::string> out;
  out.reserve(in.size());
  for (StringPiece sp : in) {
    out.push_back(sp);
  }
  return out;
}

namespace split {

Splitter fixed_length(std::size_t len) {
  return Splitter(std::make_shared<FixedSplitter>(len));
}

Splitter on(char ch) { return Splitter(std::make_shared<CharSplitter>(ch)); }

Splitter on(std::string str) {
  return Splitter(std::make_shared<StringSplitter>(std::move(str)));
}

Splitter on(Predicate pred) {
  return Splitter(std::make_shared<PredicateSplitter>(pred));
}

Splitter on_pattern(StringPiece pattern) {
  return Splitter(std::make_shared<PatternSplitter>(pattern));
}

}  // namespace split
}  // namespace base
