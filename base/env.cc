// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/env.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "base/backport.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/strings.h"
#include "base/user.h"

namespace base {
namespace env {

static bool make_is_safe() noexcept { return ::getuid() == ::geteuid(); }

bool is_safe() noexcept {
  static bool value = make_is_safe();
  return value;
}

const char* safe_get(const char* name) noexcept {
  const char* value = nullptr;
  if (is_safe()) value = ::getenv(name);
  return value;
}

static std::unique_ptr<std::string> make_home() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("HOME");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign(real_user().homedir);
  }
  return ptr;
}

const std::string& HOME() {
  static const std::string& ref = *make_home().release();
  return ref;
}

static std::unique_ptr<std::string> make_hostname() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("HOSTNAME");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    std::vector<char> buf;
    buf.resize(256);
    int rc = ::gethostname(buf.data(), buf.size() - 1);
    if (rc != 0) {
      int err_no = errno;
      auto result = Result::from_errno(err_no, "gethostname(2)");
      CHECK_OK(result);
      buf[0] = '\0';
    }
    ptr->assign(buf.data());
  }
  return ptr;
}

const std::string& HOSTNAME() {
  static const std::string& ref = *make_hostname().release();
  return ref;
}

static std::unique_ptr<std::vector<std::string>> make_path() {
  auto ptr = backport::make_unique<std::vector<std::string>>();
  const char* envvar = safe_get("PATH");
  if (envvar) {
    for (auto piece : split::on(':').omit_empty().split(envvar)) {
      ptr->push_back(piece);
    }
  } else {
    ptr->push_back("/bin");
    ptr->push_back("/usr/bin");
  }
  return ptr;
}

const std::vector<std::string>& PATH() {
  static const std::vector<std::string>& ref = *make_path().release();
  return ref;
}

static std::unique_ptr<std::string> make_shell() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("SHELL");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign("/bin/sh");
  }
  return ptr;
}

const std::string& SHELL() {
  static const std::string& ref = *make_shell().release();
  return ref;
}

static std::unique_ptr<std::string> make_term() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("TERM");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign("vt100");
  }
  return ptr;
}

const std::string& TERM() {
  static const std::string& ref = *make_term().release();
  return ref;
}

static std::unique_ptr<std::string> make_tmpdir() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("TMPDIR");
  if (!envvar) safe_get("TEMP");
  if (!envvar) safe_get("TEMPDIR");
  if (!envvar) safe_get("TMP");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign("/tmp");
  }
  return ptr;
}

const std::string& TMPDIR() {
  static const std::string& ref = *make_tmpdir().release();
  return ref;
}

static std::unique_ptr<std::string> make_tz() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("TZ");
  if (envvar) {
    ptr->assign(envvar);
  }
  return ptr;
}

const std::string& TZ() {
  static const std::string& ref = *make_tz().release();
  return ref;
}

static std::unique_ptr<std::string> make_user() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = safe_get("USER");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign(real_user().name);
  }
  return ptr;
}

const std::string& USER() {
  static const std::string& ref = *make_user().release();
  return ref;
}

static std::unique_ptr<std::vector<std::string>> make_data_dirs() {
  auto ptr = backport::make_unique<std::vector<std::string>>();
  ptr->push_back(XDG_DATA_HOME());
  const char* envvar = env::safe_get("XDG_DATA_DIRS");
  if (envvar) {
    for (auto piece : split::on(':').omit_empty().split(envvar)) {
      ptr->push_back(piece);
    }
  } else {
    ptr->push_back("/usr/local/share");
    ptr->push_back("/usr/share");
  }
  return ptr;
}

const std::vector<std::string>& XDG_DATA_DIRS() {
  static const std::vector<std::string>& ref = *make_data_dirs().release();
  return ref;
}

static std::unique_ptr<std::vector<std::string>> make_config_dirs() {
  auto ptr = backport::make_unique<std::vector<std::string>>();
  ptr->push_back(XDG_CONFIG_HOME());
  const char* envvar = env::safe_get("XDG_CONFIG_DIRS");
  if (envvar) {
    for (auto piece : split::on(':').omit_empty().split(envvar)) {
      ptr->push_back(piece);
    }
  } else {
    ptr->push_back("/etc/xdg");
  }
  return ptr;
}

const std::vector<std::string>& XDG_CONFIG_DIRS() {
  static const std::vector<std::string>& ref = *make_config_dirs().release();
  return ref;
}

static std::unique_ptr<std::string> make_runtime_dir() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = env::safe_get("XDG_RUNTIME_DIR");
  if (envvar) {
    ptr->assign(envvar);
  } else if (env::is_safe()) {
    auto pattern =
        concat("xdg-runtime-dir.user-", real_user().uid, ".XXXXXXXX");
    CHECK_OK(make_tempdir(ptr.get(), pattern.c_str()));
  } else {
    ptr->assign("/does/not/exist");
  }
  return ptr;
}

const std::string& XDG_RUNTIME_DIR() {
  static const std::string& ref = *make_runtime_dir().release();
  return ref;
}

static std::unique_ptr<std::string> make_data_home() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = env::safe_get("XDG_DATA_HOME");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign(env::HOME());
    ptr->append("/.local/share");
  }
  return ptr;
}

const std::string& XDG_DATA_HOME() {
  static const std::string& ref = *make_data_home().release();
  return ref;
}

static std::unique_ptr<std::string> make_config_home() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = env::safe_get("XDG_CONFIG_HOME");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign(env::HOME());
    ptr->append("/.config");
  }
  return ptr;
}

const std::string& XDG_CONFIG_HOME() {
  static const std::string& ref = *make_config_home().release();
  return ref;
}

static std::unique_ptr<std::string> make_cache_home() {
  auto ptr = backport::make_unique<std::string>();
  const char* envvar = env::safe_get("XDG_CACHE_HOME");
  if (envvar) {
    ptr->assign(envvar);
  } else {
    ptr->assign(env::HOME());
    ptr->append("/.cache");
  }
  return ptr;
}

const std::string& XDG_CACHE_HOME() {
  static const std::string& ref = *make_cache_home().release();
  return ref;
}

static std::string make_app_dir(const std::string& in, const char* app_name) {
  std::string out;
  out.assign(in);
  out.push_back('/');
  out.append(app_name);
  return out;
}

static std::vector<std::string> make_app_dirs(const std::vector<std::string>& in, const char* app_name) {
  std::vector<std::string> out;
  for (const std::string& basedir : in) {
    out.push_back(make_app_dir(basedir, app_name));
  }
  return out;
}

std::vector<std::string> xdg_data_dirs(const char* app_name) {
  return make_app_dirs(XDG_DATA_DIRS(), app_name);
}

std::vector<std::string> xdg_config_dirs(const char* app_name) {
  return make_app_dirs(XDG_CONFIG_DIRS(), app_name);
}

std::string xdg_runtime_dir(const char* app_name) {
  return make_app_dir(XDG_RUNTIME_DIR(), app_name);
}

std::string xdg_data_home(const char* app_name) {
  return make_app_dir(XDG_DATA_HOME(), app_name);
}

std::string xdg_config_home(const char* app_name) {
  return make_app_dir(XDG_CONFIG_HOME(), app_name);
}

std::string xdg_cache_home(const char* app_name) {
  return make_app_dir(XDG_CACHE_HOME(), app_name);
}

}  // namespace env
}  // namespace base
