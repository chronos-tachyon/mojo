// base/env.h - Safe access to environment variables
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_ENV_H
#define BASE_ENV_H

#include <string>
#include <vector>

namespace base {
namespace env {

// Returns false if the process should not trust the environment.
//
// The most common reason for this is the process running setuid.
bool is_safe() noexcept;

// Replacement for getenv(3) that respects |is_safe()|.
const char* safe_get(const char* name) noexcept;

// $HOME with safety and fallback
const std::string& HOME();

// $HOSTNAME with safety and fallback
const std::string& HOSTNAME();

// $PATH with safety and fallback
const std::vector<std::string>& PATH();

// $SHELL with safety and fallback
const std::string& SHELL();

// $TERM with safety and fallback
const std::string& TERM();

// $TMPDIR with safety and fallback
const std::string& TMPDIR();

// $TZ with safety and fallback
const std::string& TZ();

// $USER with safety and fallback
const std::string& USER();

// XDG Base Directory Specification {{{
// https://specifications.freedesktop.org/basedir-spec/latest/

const std::vector<std::string>& XDG_DATA_DIRS();
const std::vector<std::string>& XDG_CONFIG_DIRS();
const std::string& XDG_RUNTIME_DIR();
const std::string& XDG_DATA_HOME();
const std::string& XDG_CONFIG_HOME();
const std::string& XDG_CACHE_HOME();

std::vector<std::string> xdg_data_dirs(const char* app_name);
std::vector<std::string> xdg_config_dirs(const char* app_name);
std::string xdg_runtime_dir(const char* app_name);
std::string xdg_data_home(const char* app_name);
std::string xdg_config_home(const char* app_name);
std::string xdg_cache_home(const char* app_name);

// }}}

}  // namespace env
}  // namespace base

#endif  // BASE_ENV_H
