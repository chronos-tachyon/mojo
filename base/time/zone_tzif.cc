// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/zone_tzif.h"

#include <cstring>
#include <limits>
#include <tuple>

#include "base/backport.h"
#include "base/endian.h"
#include "base/logging.h"

static constexpr auto MIN = std::numeric_limits<int64_t>::min();
static constexpr auto MAX = std::numeric_limits<int64_t>::max();

namespace {
struct Header {
  uint32_t ttisgmtcnt;
  uint32_t ttisstdcnt;
  uint32_t leapcnt;
  uint32_t timecnt;
  uint32_t typecnt;
  uint32_t charcnt;
  char version;
};
}  // anonymous namespace

namespace base {
namespace time {
namespace zone {

using Mode = Recurrence::Mode;

static uint8_t consume_u8(base::StringPiece& data) {
  CHECK_GE(data.size(), 1U);
  auto num = data.bytes()[0];
  data.remove_prefix(1);
  return num;
}

static int32_t utos32(uint32_t x) {
  if (x >= 0x80000000UL)
    return -static_cast<int32_t>(~x) - 1L;
  else
    return static_cast<int32_t>(x);
}

static uint32_t consume_u32(base::StringPiece& data) {
  CHECK_GE(data.size(), 4U);
  auto num = kBigEndian->get_u32(data.data());
  data.remove_prefix(4);
  return num;
}

static int32_t consume_s32(base::StringPiece& data) {
  return utos32(consume_u32(data));
}

static int64_t utos64(uint64_t x) {
  if (x >= 0x8000000000000000ULL)
    return -static_cast<int64_t>(~x) - 1LL;
  else
    return static_cast<int64_t>(x);
}

static uint64_t consume_u64(base::StringPiece& data) {
  CHECK_GE(data.size(), 8U);
  auto num = kBigEndian->get_u64(data.data());
  data.remove_prefix(8);
  return num;
}

static int64_t consume_s64(base::StringPiece& data) {
  return utos64(consume_u64(data));
}

static int64_t consume_s3264(base::StringPiece& data, bool is_64bit) {
  if (is_64bit)
    return consume_s64(data);
  else
    return consume_s32(data);
}

static base::StringPiece consume_prefix(base::StringPiece& data,
                                        std::size_t n) {
  DCHECK_GE(data.size(), n);
  auto ret = data.substring(0, n);
  data.remove_prefix(n);
  return ret;
}

static Result consume_header(Header* out, StringPiece& data) {
  CHECK_NOTNULL(out);
  ::bzero(out, sizeof(*out));

  if (data.size() < 44)
    return Result::invalid_argument("short data for TZif file");

  if (!data.remove_prefix("TZif"))
    return Result::invalid_argument("malformed magic bytes for TZif file");

  out->version = data[0];
  consume_prefix(data, 16);

  out->ttisgmtcnt = consume_u32(data);
  out->ttisstdcnt = consume_u32(data);
  out->leapcnt = consume_u32(data);
  out->timecnt = consume_u32(data);
  out->typecnt = consume_u32(data);
  out->charcnt = consume_u32(data);

  if (out->typecnt < 1)
    return Result::invalid_argument("TZif file contains no TTInfo entries");
  if (out->typecnt > 255)
    return Result::invalid_argument(
        "TZif file contains too many TTInfo entries");
  if (out->ttisgmtcnt > out->typecnt)
    return Result::invalid_argument(
        "TZif file contains too many ttisgmt entries");
  if (out->ttisstdcnt > out->typecnt)
    return Result::invalid_argument(
        "TZif file contains too many ttisstd entries");

  return Result();
}

static std::size_t header_size(const Header& h, bool is_64bit) {
  std::size_t x = is_64bit ? 9U : 5U;
  std::size_t y = 6U;
  std::size_t z = is_64bit ? 12U : 8U;
  return (x * std::size_t(h.timecnt)) + (y * std::size_t(h.typecnt)) +
         (z * std::size_t(h.leapcnt)) + std::size_t(h.charcnt) +
         std::size_t(h.ttisgmtcnt) + std::size_t(h.ttisstdcnt);
}

static int32_t L(const std::vector<std::pair<int64_t, int32_t>>& leaps,
                 int64_t at) {
  int32_t corr = 0;
  for (const auto& pair : leaps) {
    if (at < pair.first) break;
    corr = pair.second;
  }
  return corr;
}

static Time T(const std::vector<std::pair<int64_t, int32_t>>& leaps,
              int64_t at) {
  bool neg;
  uint64_t s;
  if (at == MIN) {
    return Time::min();
  } else if (at == MAX) {
    return Time::max();
  } else {
    at -= L(leaps, at);
    if (at < 0) {
      neg = true;
      s = uint64_t(-(at + 1)) + 1;
    } else {
      neg = false;
      s = at;
    }
    return Time(Duration(internal::DurationRep(neg, s, 0)));
  }
}

static StringPiece S(StringPiece chars, std::size_t i) {
  return chars.data() + i;
}

Result parse_tzif(TZifFile* out, StringPiece filename, StringPiece data) {
  CHECK_NOTNULL(out);
  *out = TZifFile();
  out->filename = filename;

  Header h;
  auto result = consume_header(&h, data);
  if (!result) return std::move(result);
  out->version = h.version;

  std::size_t size32 = header_size(h, false);
  if (data.size() < size32) return Result::invalid_argument("short TZif data");

  bool is_64bit;
  if (h.version == 0) {
    is_64bit = false;
  } else if (h.version == '2' || h.version == '3') {
    data.remove_prefix(size32);
    result = consume_header(&h, data);
    if (!result) return std::move(result);
    std::size_t size64 = header_size(h, true);
    if (data.size() < size64)
      return Result::invalid_argument("short TZif data");
    is_64bit = true;
  } else {
    return Result::invalid_argument("unsupported TZif version");
  }

  std::vector<int64_t> times;
  times.reserve(h.timecnt);
  for (uint32_t i = 0; i < h.timecnt; ++i) {
    auto value = consume_s3264(data, is_64bit);
    if (i > 0 && times[i - 1] >= value)
      return Result::invalid_argument(
          "TZif file contains out-of-order transition times");
    times.push_back(value);
  }

  std::vector<uint8_t> indices;
  indices.reserve(h.timecnt);
  for (uint32_t i = 0; i < h.timecnt; ++i) {
    auto value = consume_u8(data);
    if (value >= h.typecnt)
      return Result::invalid_argument(
          "TZif file contains out-of-bounds TTInfo index");
    indices.push_back(value);
  }

  std::vector<std::tuple<int32_t, uint8_t, uint8_t>> types;
  types.reserve(h.typecnt);
  for (uint32_t i = 0; i < h.typecnt; ++i) {
    auto gmtoff = consume_s32(data);
    auto isdst = consume_u8(data);
    auto abbrind = consume_u8(data);
    if (isdst > 1)
      return Result::invalid_argument("TZif file contains out-of-bounds isdst");
    if (abbrind >= h.charcnt)
      return Result::invalid_argument(
          "TZif file contains out-of-bounds abbrind");
    types.emplace_back(gmtoff, isdst, abbrind);
  }

  auto chars = consume_prefix(data, h.charcnt);

  std::vector<std::pair<int64_t, int32_t>> leaps;
  leaps.reserve(h.leapcnt);
  for (uint32_t i = 0; i < h.leapcnt; ++i) {
    auto at = consume_s3264(data, is_64bit);
    auto corr = consume_s32(data);
    if (i > 0 && leaps[i - 1].first >= at)
      return Result::invalid_argument(
          "TZif file contains out-of-order leap second times");
    leaps.emplace_back(at, corr);
  }

  std::vector<bool> ttisstd;
  ttisstd.reserve(h.ttisstdcnt);
  for (uint32_t i = 0; i < h.ttisstdcnt; ++i) {
    auto value = consume_u8(data);
    if (value > 1)
      return Result::invalid_argument(
          "TZif file contains out-of-bounds ttisstd");
    ttisstd.push_back(value != 0);
  }

  std::vector<bool> ttisgmt;
  ttisgmt.reserve(h.ttisgmtcnt);
  for (uint32_t i = 0; i < h.ttisgmtcnt; ++i) {
    auto value = consume_u8(data);
    if (value > 1)
      return Result::invalid_argument(
          "TZif file contains out-of-bounds ttisgmt");
    ttisgmt.push_back(value != 0);
  }

  StringPiece spec;
  if (data.size() >= 2 && data.front() == '\n') {
    auto index = data.find('\n', 1);
    if (index != StringPiece::npos) {
      spec = data.substring(1, index - 1);
      data.remove_prefix(index + 1);
    }
  }

  if (!data.empty()) {
    static constexpr char HEX[] = "0123456789abcdef";
    auto logger = LOG(INFO);
    logger << "JUNK:";
    while (!data.empty()) {
      auto b = consume_u8(data);
      logger << " 0x" << HEX[b >> 4] << HEX[b & 15];
    }
  }

  out->times.reserve(h.timecnt);
  for (int64_t at : times) {
    out->times.push_back(T(leaps, at));
  }
  out->indices = std::move(indices);

  out->types.reserve(h.typecnt);
  for (const auto& tuple : types) {
    uint32_t gmtoff;
    uint8_t isdst, abbrind;
    std::tie(gmtoff, isdst, abbrind) = tuple;
    auto abbr = S(chars, abbrind);
    out->types.emplace_back(abbr, gmtoff, isdst != 0, true);
  }

  out->leaps.reserve(h.leapcnt);
  for (const auto& pair : leaps) {
    int64_t at;
    int32_t corr;
    std::tie(at, corr) = pair;
    out->leaps.emplace_back(T(leaps, at + 1), corr);
  }

  out->ttisstd = std::move(ttisstd);
  out->ttisgmt = std::move(ttisgmt);

  if (!spec.empty()) {
    auto posix = base::backport::make_unique<PosixRules>();
    result = parse_posix(posix.get(), spec);
    if (!result) return std::move(result);
    out->posix = std::move(posix);
  }

  return Result();
}

static std::pair<const Type*, const Type*> guess(
    const std::vector<Type>& types) {
  uint8_t x = 0, y = 0;

  bool found_x = false;
  bool found_y = false;
  for (std::size_t i = 0, n = types.size(); i < n; ++i) {
    const auto& type = types[i];
    if (type.is_specified()) {
      if (type.is_dst()) {
        if (!found_y) {
          y = i;
          found_y = true;
        }
      } else {
        if (!found_x) {
          x = i;
          found_x = true;
        }
      }
    }
  }
  if (found_x && !found_y) y = x;
  if (found_y && !found_x) x = y;
  return std::make_pair(&types[x], &types[y]);
}

static std::size_t finddupe(const std::vector<Type>& types,
                            const Type* x) noexcept {
  std::size_t i, n;
  for (i = 0, n = types.size(); i < n; ++i) {
    const Type* y = &types[i];
    if (*x == *y) break;
  }
  return i;
}

Pointer interpret_tzif(const TZifFile& in) {
  const Recurrence NEVER(Mode::never, 0, 0, 0, 0);
  const Recurrence ALWAYS(Mode::always, 0, 0, 0, 0);

  auto out = std::make_shared<Zone>();
  auto& types = out->types();
  auto& regimes = out->regimes();
  auto& leaps = out->leap_seconds();

  out->set_name(in.filename);
  types = in.types;
  leaps = in.leaps;

  auto add = [&regimes](Time t0, Time t1, Recurrence r0, Recurrence r1,
                        const Type* type0, const Type* type1) {
    regimes.emplace_back(t0, t1, r0, r1, type0, type1);
  };

  regimes.reserve(in.times.size() + 1);
  if (in.times.empty()) {
    auto pair = guess(types);
    add(Time::min(), Time::max(), NEVER, ALWAYS, pair.first, pair.second);
  } else {
    add(Time::min(), in.times.front(), NEVER, ALWAYS, &types[0], &types[0]);

    std::size_t n = in.times.size() - 1;
    for (std::size_t i = 0; i < n; ++i) {
      auto r0 = NEVER;
      auto r1 = ALWAYS;
      const auto* type0 = &types[in.indices[i]];
      const auto* type1 = &types[in.indices[i + 1]];
      if (type0->is_dst()) {
        std::swap(r0, r1);
        std::swap(type0, type1);
      }
      add(in.times[i], in.times[i + 1], r0, r1, type0, type1);
    }

    Recurrence r0, r1;
    const Type *type0, *type1;
    if (in.posix) {
      r0 = in.posix->dst_start;
      r1 = in.posix->dst_end;

      auto i = finddupe(types, &in.posix->standard_time);
      if (i == types.size()) types.push_back(in.posix->standard_time);
      type0 = &types[i];

      auto j = finddupe(types, &in.posix->daylight_time);
      if (j == types.size()) types.push_back(in.posix->daylight_time);
      type1 = &types[j];
    } else {
      r0 = NEVER;
      r1 = ALWAYS;
      type0 = type1 = &types[in.indices[n]];
      if (type0->is_dst()) std::swap(r0, r1);
    }
    add(in.times[n], Time::max(), r0, r1, type0, type1);
  }

  return out;
}

}  // namespace zone
}  // namespace time
}  // namespace base
