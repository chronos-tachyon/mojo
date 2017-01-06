#include "base/options.h"

namespace base {

Options::Options(const Options& other) {
  for (const auto& pair : other.map_) {
    map_[pair.first] = pair.second->copy();
  }
}

Options& Options::operator=(const Options& other) {
  map_.clear();
  for (const auto& pair : other.map_) {
    map_[pair.first] = pair.second->copy();
  }
  return *this;
}

}  // namespace base
