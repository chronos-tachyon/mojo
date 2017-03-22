// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <iostream>

#include "base/env.h"
#include "base/strings.h"

int main(int argc, char** argv) {
  auto joiner = base::join::on(':');
  auto J = [&joiner](const std::vector<std::string>& v) -> std::string {
    return joiner.join(v);
  };

  std::cout << "HOME=" << base::env::HOME() << "\n"
            << "HOSTNAME=" << base::env::HOSTNAME() << "\n"
            << "PATH=" << J(base::env::PATH()) << "\n"
            << "SHELL=" << base::env::SHELL() << "\n"
            << "TERM=" << base::env::TERM() << "\n"
            << "TMPDIR=" << base::env::TMPDIR() << "\n"
            << "TZ=" << base::env::TZ() << "\n"
            << "USER=" << base::env::USER() << "\n"
            << "XDG_DATA_DIRS=" << J(base::env::XDG_DATA_DIRS()) << "\n"
            << "XDG_CONFIG_DIRS=" << J(base::env::XDG_CONFIG_DIRS()) << "\n"
            << "XDG_RUNTIME_DIR=" << base::env::XDG_RUNTIME_DIR() << "\n"
            << "XDG_DATA_HOME=" << base::env::XDG_DATA_HOME() << "\n"
            << "XDG_CONFIG_HOME=" << base::env::XDG_CONFIG_HOME() << "\n"
            << "XDG_CACHE_HOME=" << base::env::XDG_CACHE_HOME() << "\n"
            << std::endl;

  std::cout << "foo_data_dirs=" << J(base::env::xdg_data_dirs("foo")) << "\n"
            << "foo_config_dirs=" << J(base::env::xdg_config_dirs("foo")) << "\n"
            << "foo_runtime_dir=" << base::env::xdg_runtime_dir("foo") << "\n"
            << "foo_data_home=" << base::env::xdg_data_home("foo") << "\n"
            << "foo_config_home=" << base::env::xdg_config_home("foo") << "\n"
            << "foo_cache_home=" << base::env::xdg_cache_home("foo") << "\n"
            << std::endl;

  return 0;
}
