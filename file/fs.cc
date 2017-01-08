// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/fs.h"

#include <algorithm>
#include <stdexcept>

#include "base/logging.h"

namespace file {

void File::assert_valid() const noexcept {
  CHECK(ptr_) << ": file::File is empty";
}

}  // namespace file
