// net/internal.h - Private helper functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_INTERNAL_H
#define NET_INTERNAL_H

#include <cstdint>

namespace net {
namespace internal {

std::size_t hash(const void* ptr, std::size_t len) noexcept;

std::size_t mix(std::size_t a, std::size_t b) noexcept;

}  // namespace internal
}  // namespace net

#endif  // NET_INTERNAL_H
