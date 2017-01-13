// file/registry.h - Registers the installed filesystems
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_REGISTRY_H
#define FILE_REGISTRY_H

#include <cstdint>
#include <vector>

#include "file/fs.h"

namespace file {

// Registry is a clearinghouse for registering and finding filesystems.
class Registry {
 public:
  // Indicates a priority for a FileSystemImpl.
  // Larger numbers indicate a higher priority.
  // System filesystems are installed at priority 50.
  using prio_t = unsigned int;

  // Registry is default constructible.
  Registry() = default;

  // Registry is copyable and moveable.
  Registry(const Registry&) = default;
  Registry(Registry&&) noexcept = default;
  Registry& operator=(const Registry&) = default;
  Registry& operator=(Registry&&) noexcept = default;

  // Swaps this Registry with another.
  void swap(Registry& x) noexcept { items_.swap(x.items_); }

  // Returns true iff this Registry has a non-empty set of FileSystemImpls.
  explicit operator bool() const noexcept { return !items_.empty(); }

  // Registers a FileSystemImpl at priority |prio|.
  // - If |t| is provided, it is set to a token identifying this registration
  void add(base::token_t* /*nullable*/ t, prio_t prio, FileSystemPtr ptr);

  // Undoes the previous registration that yielded |t|.
  void remove(base::token_t t);

  // Finds the FileSystemImpl that implements |fsname|, or else returns NULL.
  FileSystemPtr find(const std::string& fsname) const;

 private:
  struct Item {
    prio_t prio;
    base::token_t token;
    FileSystemPtr ptr;

    Item(prio_t prio, base::token_t t, FileSystemPtr ptr) noexcept
        : prio(prio),
          token(t),
          ptr(std::move(ptr)) {}
    Item() noexcept : prio(0), token(), ptr() {}
  };

  friend inline bool operator<(const Item& a, const Item& b) {
    return (a.prio > b.prio) || (a.prio == b.prio && a.token < b.token);
  }

  std::vector<Item> items_;
};

// Registry is swappable.
inline void swap(Registry& a, Registry& b) noexcept { a.swap(b); }

// Accesses the process-wide Registry.
std::mutex& system_registry_mutex();
Registry& system_registry_mutable();
const Registry& system_registry();

}  // namespace file

#endif  // FILE_REGISTRY_H
