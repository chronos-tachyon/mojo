// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/registry.h"

#include <algorithm>

namespace file {

void Registry::add(base::token_t* t, prio_t prio, FileSystemPtr ptr) {
  base::token_t token = base::next_token();
  items_.emplace_back(prio, token, std::move(ptr));
  std::sort(items_.begin(), items_.end());
  if (t) *t = token;
}

void Registry::remove(base::token_t t) {
  auto it = items_.end(), begin = items_.begin();
  while (it != begin) {
    --it;
    if (it->token == t) {
      items_.erase(it);
      break;
    }
  }
}

FileSystemPtr Registry::find(const std::string& fsname) const {
  for (const auto& item : items_) {
    if (item.ptr->name() == fsname) return item.ptr;
  }
  return nullptr;
}

std::mutex& system_registry_mutex() {
  static std::mutex mu;
  return mu;
}

Registry& system_registry_mutable() {
  static Registry& ref = *new Registry;
  return ref;
}

const Registry& system_registry() { return system_registry_mutable(); }

}  // namespace file
