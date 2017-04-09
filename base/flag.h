// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_FLAGS_H
#define BASE_FLAGS_H

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/backport.h"
#include "base/chars.h"
#include "base/concat.h"
#include "base/result.h"

namespace base {

class FlagSet;  // forward declaration

enum class FlagArgument : unsigned char {
  none = 0,
  required = 1,
  optional = 2,
};

using FlagSetter = std::function<base::Result(FlagSet*, bool, base::Chars)>;

struct FlagHook {
  std::string name;
  FlagArgument arg;
  FlagSetter setter;

  FlagHook(std::string name, FlagArgument arg, FlagSetter setter)
      : name(std::move(name)), arg(arg), setter(std::move(setter)) {}
};

class Flag {
 protected:
  using HookVec = std::vector<std::unique_ptr<FlagHook>>;

  Flag(base::Chars name, base::Chars help);

  const HookVec& hooks() const noexcept { return hooks_; }
  HookVec& hooks() noexcept { return hooks_; }

  void push_hook(std::string name, FlagArgument arg, FlagSetter setter) {
    hooks_.push_back(base::backport::make_unique<FlagHook>(std::move(name), arg,
                                                           std::move(setter)));
  }

 public:
  virtual ~Flag() noexcept = default;
  virtual void add_alias(base::Chars name) = 0;
  virtual bool is_set() const noexcept = 0;
  virtual std::string get() const = 0;
  virtual std::string get_default() const = 0;
  virtual void reset() = 0;
  virtual base::Result set(base::Chars value) = 0;

  base::Chars name() const noexcept { return name_; }
  base::Chars help() const noexcept { return help_; }
  bool is_required() const noexcept { return required_; }

  Flag& mark_required() noexcept {
    required_ = true;
    return *this;
  }
  Flag& alias(base::Chars name) {
    add_alias(name);
    return *this;
  }

 private:
  friend class FlagSet;

  std::string name_;
  std::string help_;
  HookVec hooks_;
  bool required_;
};

class HelpFlag : public Flag {
 public:
  HelpFlag();
  void add_alias(base::Chars name) override;
  bool is_set() const noexcept override;
  std::string get() const override;
  std::string get_default() const override;
  void reset() override;
  base::Result set(base::Chars value) override;
};

class VersionFlag : public Flag {
 public:
  VersionFlag();
  void add_alias(base::Chars name) override;
  bool is_set() const noexcept override;
  std::string get() const override;
  std::string get_default() const override;
  void reset() override;
  base::Result set(base::Chars value) override;
};

class BoolFlag : public Flag {
 public:
  BoolFlag(base::Chars name, bool default_value, base::Chars help);
  void add_alias(base::Chars name) override;
  bool is_set() const noexcept override;
  std::string get() const override;
  std::string get_default() const override;
  void reset() override;
  base::Result set(base::Chars value) override;

  bool value() const noexcept { return value_; }
  void set_value(bool value) noexcept {
    value_ = value;
    isset_ = true;
  }

 private:
  bool default_;
  bool value_;
  bool isset_;
};

class StringFlag : public Flag {
 public:
  StringFlag(base::Chars name, base::Chars default_value, base::Chars help);
  void add_alias(base::Chars name) override;
  bool is_set() const noexcept override;
  std::string get() const override;
  std::string get_default() const override;
  void reset() override;
  base::Result set(base::Chars value) override;

  base::Chars value() const noexcept { return value_; }
  void set_value(base::Chars value) {
    value_ = value;
    isset_ = true;
  }

 private:
  std::string default_;
  std::string value_;
  bool isset_;
};

class ChoiceFlag : public StringFlag {
 public:
  ChoiceFlag(base::Chars name, std::vector<base::Chars> choices,
             base::Chars default_value, base::Chars help);
  base::Result set(base::Chars value) override;

  const std::vector<std::string>& choices() const noexcept { return choices_; }

 private:
  std::vector<std::string> choices_;
};

class FlagSet {
 public:
  FlagSet();

  void register_hook(FlagHook* hook);

  Flag& add(std::unique_ptr<Flag> flag);
  Flag& add_help();
  Flag& add_version();
  Flag& add_bool(base::Chars name, bool default_value, base::Chars help);
  Flag& add_string(base::Chars name, base::Chars default_value,
                   base::Chars help);
  Flag& add_choice(base::Chars name, std::vector<base::Chars> choices,
                   base::Chars default_value, base::Chars help);

  void set_program_name(base::Chars progname) { progname_ = progname; }
  void set_version(base::Chars version) { version_ = version; }
  void set_description(base::Chars description) { description_ = description; }
  void set_usage(base::Chars usage) { usage_ = usage; }
  void set_prologue(base::Chars prologue) { prologue_ = prologue; }
  void set_epilogue(base::Chars epilogue) { epilogue_ = epilogue; }

  base::Chars program_name() const noexcept { return progname_; }
  base::Chars version() const noexcept { return version_; }
  base::Chars description() const noexcept { return description_; }
  base::Chars usage() const noexcept { return usage_; }
  base::Chars prologue() const noexcept { return prologue_; }
  base::Chars epilogue() const noexcept { return epilogue_; }

  const std::vector<std::unique_ptr<Flag>>& flags() const noexcept {
    return flags_;
  }

  Flag* get(base::Chars name) const noexcept;

  BoolFlag* get_bool(base::Chars name) const noexcept {
    return dynamic_cast<BoolFlag*>(get(name));
  }

  StringFlag* get_string(base::Chars name) const noexcept {
    return dynamic_cast<StringFlag*>(get(name));
  }

  ChoiceFlag* get_choice(base::Chars name) const noexcept {
    return dynamic_cast<ChoiceFlag*>(get(name));
  }

  const std::vector<base::Chars>& args() const noexcept { return args_; }

  void show_help(std::ostream& o);
  void show_version(std::ostream& o);

  void parse(int argc, const char* const* argv);

  __attribute__((noreturn)) void die(base::Chars msg);

  template <typename... Args>
  __attribute__((noreturn)) void die(Args&&... args) {
    die(concat(std::forward<Args>(args)...));
  }

 private:
  std::string progname_;
  std::string version_;
  std::string description_;
  std::string usage_;
  std::string prologue_;
  std::string epilogue_;
  std::vector<std::unique_ptr<Flag>> flags_;
  std::map<base::Chars, Flag*> names_;
  std::map<base::Chars, FlagHook*> hooks_;
  std::vector<base::Chars> args_;
};

}  // namespace base

#endif  // BASE_FLAGS_H
