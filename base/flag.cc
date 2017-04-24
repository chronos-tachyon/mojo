#include "base/flag.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "base/logging.h"

namespace base {

Flag::Flag(base::Chars name, base::Chars help)
    : name_(name), help_(help), required_(false) {}

HelpFlag::HelpFlag() : Flag("help", "Shows this usage information") {
  add_alias("?");
  add_alias("h");
  add_alias("help");
}
void HelpFlag::add_alias(base::Chars name) {
  auto f = [this](FlagSet* flagset, bool, base::Chars) -> base::Result {
    flagset->show_help(std::cout);
    exit(0);
  };
  push_hook(name, FlagArgument::none, f);
}
bool HelpFlag::is_set() const noexcept { return false; }
std::string HelpFlag::get() const { return ""; }
std::string HelpFlag::get_default() const { return ""; }
void HelpFlag::reset() {}
base::Result HelpFlag::set(base::Chars value) {
  return base::Result::invalid_argument("--help does not take a value");
}

VersionFlag::VersionFlag() : Flag("version", "Shows version information") {
  add_alias("V");
  add_alias("version");
}
void VersionFlag::add_alias(base::Chars name) {
  auto f = [this](FlagSet* flagset, bool, base::Chars) -> base::Result {
    flagset->show_version(std::cout);
    exit(0);
  };
  push_hook(name, FlagArgument::none, f);
}
bool VersionFlag::is_set() const noexcept { return false; }
std::string VersionFlag::get() const { return ""; }
std::string VersionFlag::get_default() const { return ""; }
void VersionFlag::reset() {}
base::Result VersionFlag::set(base::Chars value) {
  return base::Result::invalid_argument("--version does not take a value");
}

BoolFlag::BoolFlag(base::Chars name, bool default_value,
                   base::Chars help)
    : Flag(name, help),
      default_(default_value),
      value_(default_value),
      isset_(false) {
  add_alias(name);
}
void BoolFlag::add_alias(base::Chars name) {
  auto f = [this](FlagSet*, bool have_value, base::Chars value) {
    if (!have_value) value = "true";
    return set(value);
  };

  auto g = [this](FlagSet*, bool, base::Chars) { return set("false"); };

  std::string negation;
  negation.append("no");
  name.append_to(&negation);

  push_hook(name, FlagArgument::optional, f);
  push_hook(negation, FlagArgument::none, g);
}
bool BoolFlag::is_set() const noexcept { return isset_; }
std::string BoolFlag::get() const {
  if (value_)
    return "true";
  else
    return "false";
}
std::string BoolFlag::get_default() const {
  if (default_)
    return "true";
  else
    return "false";
}
void BoolFlag::reset() {
  value_ = default_;
  isset_ = false;
}
base::Result BoolFlag::set(base::Chars value) {
  using Map = std::map<base::Chars, bool>;
  static Map& map = *new Map{
      {"0", false},     {"1", true},    {"f", false}, {"t", true},
      {"false", false}, {"true", true}, {"n", false}, {"y", true},
      {"no", false},    {"yes", true},
  };

  auto it = map.find(value);
  if (it == map.end())
    return base::Result::invalid_argument("invalid boolean value");
  value_ = it->second;
  isset_ = true;
  return base::Result();
}

StringFlag::StringFlag(base::Chars name, base::Chars default_value,
                       base::Chars help)
    : Flag(name, help),
      default_(default_value),
      value_(default_value),
      isset_(false) {
  add_alias(name);
}
void StringFlag::add_alias(base::Chars name) {
  auto f = [this](FlagSet*, bool, base::Chars value) {
    return set(value);
  };

  push_hook(name, FlagArgument::required, f);
}
bool StringFlag::is_set() const noexcept { return isset_; }
std::string StringFlag::get() const { return value_; }
std::string StringFlag::get_default() const { return default_; }
void StringFlag::reset() {
  value_ = default_;
  isset_ = false;
}
base::Result StringFlag::set(base::Chars value) {
  value_ = value;
  isset_ = true;
  return base::Result();
}

template <typename T, typename U>
static std::vector<T> conv(std::vector<U> in) {
  std::vector<T> out;
  out.reserve(in.size());
  for (auto& item : in) {
    out.push_back(std::move(item));
  }
  return out;
}

ChoiceFlag::ChoiceFlag(base::Chars name,
                       std::vector<base::Chars> choices,
                       base::Chars default_value, base::Chars help)
    : StringFlag(name, default_value, help),
      choices_(conv<std::string>(std::move(choices))) {}

base::Result ChoiceFlag::set(base::Chars value) {
  bool found = false;
  for (const auto& choice : choices_) {
    if (value == choice) {
      found = true;
      break;
    }
  }
  if (found)
    return this->StringFlag::set(value);
  else
    return base::Result::invalid_argument("invalid choice value");
}

FlagSet::FlagSet()
    : progname_("<program>"), version_("unknown"), usage_("<flags>") {}

void FlagSet::register_hook(FlagHook* hook) {
  hooks_[hook->name] = hook;
}

Flag& FlagSet::add(std::unique_ptr<Flag> flag) {
  CHECK_NOTNULL(flag.get());
  Flag* ptr = flag.get();
  flags_.push_back(std::move(flag));
  names_[ptr->name()] = ptr;
  for (const auto& hook : ptr->hooks_) {
    register_hook(hook.get());
  }
  return *ptr;
}

Flag& FlagSet::add_help() {
  return add(base::backport::make_unique<HelpFlag>());
}

Flag& FlagSet::add_version() {
  return add(base::backport::make_unique<VersionFlag>());
}

Flag& FlagSet::add_bool(base::Chars name, bool default_value,
                        base::Chars help) {
  return add(base::backport::make_unique<BoolFlag>(name, default_value, help));
}

Flag& FlagSet::add_string(base::Chars name,
                          base::Chars default_value,
                          base::Chars help) {
  return add(
      base::backport::make_unique<StringFlag>(name, default_value, help));
}

Flag& FlagSet::add_choice(base::Chars name,
                          std::vector<base::Chars> choices,
                          base::Chars default_value,
                          base::Chars help) {
  return add(base::backport::make_unique<ChoiceFlag>(name, std::move(choices),
                                                     default_value, help));
}

Flag* FlagSet::get(base::Chars name) const noexcept {
  auto it = names_.find(name);
  if (it != names_.end())
    return it->second;
  else
    return nullptr;
}

void FlagSet::show_help(std::ostream& o) {
  if (!description_.empty()) {
    o << description_ << "\n";
  }

  o << "Usage: " << progname_ << " " << usage_ << "\n\n";

  if (!prologue_.empty()) {
    o << prologue_ << "\n\n";
  }

  o << "Flags:\n";
  std::size_t longest = 0;
  for (const auto& flag : flags_) {
    auto n = flag->name().size();
    if (longest < n) longest = n;
  }
  for (const auto& flag : flags_) {
    o << "  --" << std::left << std::setw(longest) << flag->name() << "  "
      << flag->help();

    ChoiceFlag* choiceptr = dynamic_cast<ChoiceFlag*>(flag.get());
    if (choiceptr != nullptr) {
      o << " [choices: ";
      bool first = true;
      for (const auto& choice : choiceptr->choices()) {
        if (first)
          first = false;
        else
          o << ',';
        o << choice;
      }
      o << "]";
    }

    if (flag->is_required()) {
      o << " [required]\n";
    } else {
      auto def = flag->get_default();
      if (def.empty())
        o << "\n";
      else
        o << " [default: " << def << "]\n";
    }
  }
  o << "\n";

  if (!epilogue_.empty()) {
    o << epilogue_ << "\n\n";
  }

  o << std::flush;
}

void FlagSet::show_version(std::ostream& o) { o << version_ << std::endl; }

void FlagSet::die(base::Chars msg) {
  std::cerr << "ERROR: " << msg << std::endl;
  exit(2);
}

void FlagSet::parse(int argc, const char* const* argv) {
  if (argc > 0 && progname_ == "<program>") progname_ = argv[0];

  int i = 1;
  while (i < argc) {
    base::Chars arg(argv[i]);
    ++i;

    if (!arg.has_prefix("-")) {
      args_.push_back(arg);
      continue;
    }

    if (arg == "--") break;
    arg.remove_prefix("-");
    arg.remove_prefix("-");
    Chars flag;
    auto index = arg.find('=');
    bool have_arg;
    if (index == Chars::npos) {
      flag = arg;
      arg = Chars();
      have_arg = false;
    } else {
      flag = arg.substring(0, index);
      arg.remove_prefix(index + 1);
      have_arg = true;
    }

    auto it = hooks_.find(flag);
    if (it == hooks_.end()) {
      die(concat("unknown flag: --", flag));
    }

    FlagHook& hook = *it->second;

    if (!have_arg && hook.arg == FlagArgument::required) {
      if (i >= argc) {
        die(concat("missing required argument for flag --", flag));
      }
      have_arg = true;
      arg = argv[i];
      ++i;
    }

    if (have_arg && hook.arg == FlagArgument::none) {
      die(concat("flag --", flag, " does not take an argument"));
    }

    auto result = hook.setter(this, have_arg, arg);
    if (!result) {
      die(concat("--", flag, ": ", result));
    }
  }
  while (i < argc) {
    args_.push_back(argv[i]);
    ++i;
  }

  for (const auto& flag : flags_) {
    if (flag->is_required() && !flag->is_set()) {
      die(concat("missing required flag --", flag->name()));
    }
  }
}

}  // namespace base
