#include <iostream>

#include "base/flag.h"

int main(int argc, char** argv) {
  base::FlagSet flags;
  flags.set_description("Demonstrates use of Mojo's base/flag.h");
  flags.set_prologue("I am a prologue.");
  flags.set_epilogue("I am an epilogue.");
  flags.add_help();
  flags.add_version();
  flags.add_bool("foo", false, "Help for --foo").mark_required();
  flags.add_bool("bar", false, "Help for --bar");
  flags.add_string("baz", "", "Help for --baz");
  flags.add_string("quux", "xxx", "Help for --quux");
  flags.add_choice("flintstone", {"fred", "wilma", "pebbles"}, "", "Help for --flintstone");
  flags.parse(argc, argv);
  std::cout << std::boolalpha;
  std::cout << "foo=" << flags.get_bool("foo")->value() << "\n"
            << "bar=" << flags.get_bool("bar")->value() << "\n"
            << "baz=" << flags.get_string("baz")->value() << "\n"
            << "quux=" << flags.get_string("quux")->value() << "\n"
            << "flintstone=" << flags.get_choice("flintstone")->value() << "\n";
  const auto& args = flags.args();
  for (std::size_t i = 0; i < args.size(); ++i) {
    std::cout << "args[" << i << "]=" << args[i] << "\n";
  }
  std::cout << std::flush;
  return 0;
}
