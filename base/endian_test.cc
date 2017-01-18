#include "gtest/gtest.h"

#include <initializer_list>

#include "base/endian.h"

static constexpr uint8_t kU8 = 0x01U;
static constexpr uint16_t kU16 = 0x0201U;
static constexpr uint32_t kU32 = 0x04030201UL;
static constexpr uint64_t kU64 = 0x0807060504030201ULL;

static const char kHex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static std::string examine(const char* buf, std::size_t len) {
  std::string out;
  const auto* ptr = reinterpret_cast<const unsigned char*>(buf);
  const auto* end = ptr + len;
  while (ptr != end) {
    unsigned char ch = *ptr;
    out.push_back(kHex[(ch >> 4) & 0xf]);
    out.push_back(kHex[ch & 0xf]);
    out.push_back('-');
    ++ptr;
  }
  if (!out.empty()) out.resize(out.size() - 1);
  return out;
}

TEST(Endian, Big) {
  char buf[8];
  ::bzero(buf, sizeof(buf));

  const auto* const endian = base::kBigEndian;

  endian->put_u16(buf, kU16);
  EXPECT_EQ("02-01-00-00-00-00-00-00", examine(buf, sizeof(buf)));
  uint16_t u16 = endian->get_u16(buf);
  EXPECT_EQ(kU16, u16);

  endian->put_u32(buf, kU32);
  EXPECT_EQ("04-03-02-01-00-00-00-00", examine(buf, sizeof(buf)));
  uint32_t u32 = endian->get_u32(buf);
  EXPECT_EQ(kU32, u32);

  endian->put_u64(buf, kU64);
  EXPECT_EQ("08-07-06-05-04-03-02-01", examine(buf, sizeof(buf)));
  uint64_t u64 = endian->get_u64(buf);
  EXPECT_EQ(kU64, u64);
}

TEST(Endian, Little) {
  char buf[8];
  ::bzero(buf, sizeof(buf));

  const auto* const endian = base::kLittleEndian;

  endian->put_u16(buf, kU16);
  EXPECT_EQ("01-02-00-00-00-00-00-00", examine(buf, sizeof(buf)));
  uint16_t u16 = endian->get_u16(buf);
  EXPECT_EQ(kU16, u16);

  endian->put_u32(buf, kU32);
  EXPECT_EQ("01-02-03-04-00-00-00-00", examine(buf, sizeof(buf)));
  uint32_t u32 = endian->get_u32(buf);
  EXPECT_EQ(kU32, u32);

  endian->put_u64(buf, kU64);
  EXPECT_EQ("01-02-03-04-05-06-07-08", examine(buf, sizeof(buf)));
  uint64_t u64 = endian->get_u64(buf);
  EXPECT_EQ(kU64, u64);
}

static void TestEndianObject(const base::Endian* endian) {
  char buf[8];
  ::bzero(buf, sizeof(buf));

  endian->put_u16(buf, kU16);
  uint16_t u16 = endian->get_u16(buf);
  EXPECT_EQ(kU16, u16);

  endian->put_u32(buf, kU32);
  uint32_t u32 = endian->get_u32(buf);
  EXPECT_EQ(kU32, u32);

  endian->put_u64(buf, kU64);
  uint64_t u64 = endian->get_u64(buf);
  EXPECT_EQ(kU64, u64);
}

TEST(Endian, BigObject) { TestEndianObject(base::kBigEndian); }

TEST(Endian, LittleObject) { TestEndianObject(base::kLittleEndian); }

TEST(Endian, NativeObject) { TestEndianObject(base::kNativeEndian); }
