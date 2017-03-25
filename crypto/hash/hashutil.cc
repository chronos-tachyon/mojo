// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <unistd.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "base/concat.h"
#include "base/logging.h"
#include "base/result.h"
#include "crypto/hash/hash.h"

static void usage(std::ostream* o) {
  *o << "Driver for testing cryptographic hash implementations\n"
     << "Usage:\n"
     << "  hashutil [-broken | -weak] -list\n"
     << "  hashutil [-broken | -weak] <algo>[:d=<d>|:n=<n>]\n"
     << "\n"
     << "Flags:\n"
     << "  -help         show this usage information\n"
     << "  -broken       permit algorithms with known breaks\n"
     << "  -weak         permit algorithms known or suspected to be weak\n"
     << "  -list         list all available algorithms\n"
     << "\n"
     << "<algo> is algorithm name\n"
     << "<d> is hash output length in *bits*\n"
     << "<n> is hash output length in *bytes*\n"
     << "\n";
}

template <typename... Args>
static __attribute__((noreturn)) void die(Args&&... args) {
  std::cerr << "ERROR: " << base::concat(std::forward<Args>(args)...)
            << std::endl;
  exit(2);
}

static unsigned int parse_uint(const std::string& str) {
  const char* ptr = str.c_str();
  const char* end = ptr;
  auto x = ::strtoul(ptr, const_cast<char**>(&end), 10);
  CHECK(ptr != end && !*end) << ": failed to parse integer \"" << str << "\"";
  return x;
}

int main(int argc, char** argv) {
  crypto::hash::Security min_security = crypto::hash::Security::secure;
  bool do_list = false;

  int i = 1;
  while (i < argc && argv[i][0] == '-') {
    std::string flag = argv[i];
    if (flag == "-h" || flag == "-help" || flag == "--help") {
      usage(&std::cout);
      std::cout << std::flush;
      return 0;
    } else if (flag == "-broken" || flag == "--broken") {
      min_security = crypto::hash::Security::broken;
    } else if (flag == "-weak" || flag == "--weak") {
      min_security = crypto::hash::Security::weak;
    } else if (flag == "-list" || flag == "--list") {
      do_list = true;
    } else {
      usage(&std::cerr);
      die("unknown flag: ", flag);
    }
    ++i;
  }

  if (do_list) {
    if (i < argc) {
      usage(&std::cerr);
      die("unexpected extra arguments");
    }
    std::cout << std::left;
    std::cout << std::setw(24) << "ALGORITHM"
              << " " << std::setw(8) << "SECURITY"
              << " " << std::setw(10) << "BLOCK SIZE"
              << " " << std::setw(13) << "OUTPUT LENGTH"
              << "\n";
    for (const auto* algo : crypto::hash::all(min_security)) {
      char ch = (algo->newvarlenfn ? 'V' : ' ');
      std::cout << std::left << std::setw(24) << algo->name << ""
                << " " << std::setw(8) << security_name(algo->security) << ""
                << " " << std::right << std::setw(10) << algo->block_size << ""
                << " " << std::setw(11) << algo->size << ' ' << ch << ""
                << "\n";
    }
    std::cout << "\n"
              << "V = variable output length\n";
    std::cout << std::flush;
    return 0;
  }

  std::string name;
  if (i < argc) {
    name = argv[i];
    ++i;
  } else {
    usage(&std::cerr);
    die("missing required argument <algo>");
  }

  if (i < argc) {
    usage(&std::cerr);
    die("unexpected extra arguments");
  }

  bool use_variable_length = false;
  unsigned int variable_length = 0;
  auto index = name.find(":d=");
  if (index != std::string::npos) {
    auto d = parse_uint(name.substr(index + 3));
    CHECK((d & 7) == 0) << ": " << d << " not a multiple of 8";
    use_variable_length = true;
    variable_length = d / 8;
    name.erase(index);
  } else {
    index = name.find(":n=");
    if (index != std::string::npos) {
      auto n = parse_uint(name.substr(index + 3));
      use_variable_length = true;
      variable_length = n;
      name.erase(index);
    }
  }

  const auto* algo = crypto::hash::by_name(name, min_security);
  if (!algo) {
    die("unknown hash algorithm: ", name);
  }

  std::unique_ptr<crypto::hash::State> state;
  if (use_variable_length) {
    if (!algo->newvarlenfn) {
      die(algo->name, " does not support variable-length output");
    }
    state = algo->newvarlenfn(variable_length);
  } else {
    state = algo->newfn();
  }

  while (true) {
    char buf[4096];
    ssize_t n = ::read(0, buf, sizeof(buf));
    if (n < 0) {
      int err_no = errno;
      if (err_no == EINTR) continue;
      auto result = base::Result::from_errno(err_no, "read(2) path=/dev/stdin");
      LOG(ERROR) << result;
      return 1;
    }
    if (n == 0) break;
    state->write(base::StringPiece(buf, n));
  }
  state->finalize();
  std::cout << state->sum_hex() << std::endl;
}
