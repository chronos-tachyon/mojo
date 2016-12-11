// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/options.h"

namespace io {

const Options& default_options() noexcept {
  return mutable_default_options();
}

Options& mutable_default_options() noexcept {
  static Options& ref = *new Options;
  return ref;
}

}  // namespace io
