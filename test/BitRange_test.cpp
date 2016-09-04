#include <gtest/gtest.h>
#include <boost/dynamic_bitset.hpp>
#include <boost/detail/endian.hpp>

#include "util/BitRange.hpp"

static_assert(BOOST_BYTE_ORDER == 1234,
  "Following test designed for little-endian machines only."
  " Their versions for other endianness are to be implemented.");


namespace
{
  template<typename T, size_t size>
  bool RangesEqual(T(& actual)[size], boost::dynamic_bitset<unsigned char> const & expected)
  {
    std::vector<char> expected_ch;
    boost::to_block_range(expected, std::back_inserter(expected_ch));
    const char * actual_ch = reinterpret_cast<const char *>(actual);
    return std::equal(actual_ch, actual_ch + sizeof(T) * size, 
      expected_ch.begin(), expected_ch.end());
  }
}

template<class T>
inline void ClearBitRangeTestT(int start, int end)
{
  static const int ItemsCount = 32 / sizeof(T);
  T range[ItemsCount];
  std::fill(std::begin(range), std::end(range), std::numeric_limits<T>::max());
  f2f::util::ClearBitRange(range, start, end);

  boost::dynamic_bitset<unsigned char> expected1;
  expected1.resize(sizeof(range) * 8, true);
  for (int i = start; i <= end; ++i)
    expected1.reset(i);
  EXPECT_TRUE(RangesEqual(range, expected1));
}

class ClearBitRangeTest : public ::testing::TestWithParam<std::pair<int, int>> {
};

TEST_P(ClearBitRangeTest, ClearBitRange)
{
  ClearBitRangeTestT<uint32_t>(GetParam().first, GetParam().second);
  ClearBitRangeTestT<uint64_t>(GetParam().first, GetParam().second);
}

template<class T>
inline void SetBitRangeTestT(int start, int end)
{
  static const int ItemsCount = 32 / sizeof(T);
  T range[ItemsCount] = {};
  f2f::util::SetBitRange(range, start, end);

  boost::dynamic_bitset<unsigned char> expected1;
  expected1.resize(sizeof(range) * 8, false);
  for (int i = start; i <= end; ++i)
    expected1.set(i);
  EXPECT_TRUE(RangesEqual(range, expected1));
}

class SetBitRangeTest : public ::testing::TestWithParam<std::pair<int, int>> {
};


TEST_P(SetBitRangeTest, SetBitRange)
{
  SetBitRangeTestT<uint32_t>(GetParam().first, GetParam().second);
  SetBitRangeTestT<uint64_t>(GetParam().first, GetParam().second);
}

auto values = ::testing::Values(
  std::make_pair(32, 37),
  std::make_pair(0, 7 * 32 - 1),
  std::make_pair(5, 172),
  std::make_pair(0, 63),
  std::make_pair(0, 64),
  std::make_pair(33, 129),
  std::make_pair(31, 128),
  std::make_pair(32, 127),
  std::make_pair(45, 45)
);

INSTANTIATE_TEST_CASE_P(BitTest,
  ClearBitRangeTest,
  values
);

INSTANTIATE_TEST_CASE_P(BitTest,
  SetBitRangeTest,
  values
);

template<class T>
inline void FindAndSetFirstZeroBitT()
{
  uint32_t range32[8];
  T * range = reinterpret_cast<T *>(range32);
  static const int WordCount = sizeof(range32)/sizeof(T);

  std::fill(std::begin(range32), std::end(range32), 0xFFFFFFFFui32);
  int nextWordWithZeroBit;
  EXPECT_EQ(-1, f2f::util::FindAndSetFirstZeroBit(range, 0, WordCount, nextWordWithZeroBit));
  range32[2] = 0b1111'1111'1011'1111'1111'1011'1111'1111ui32;
  range32[3] = 0b1111'1111'1111'1111'1111'1111'1111'1110ui32;
  EXPECT_EQ(2 * 32 + 10, f2f::util::FindAndSetFirstZeroBit(range, 0, WordCount, nextWordWithZeroBit));
  EXPECT_EQ(0b1111'1111'1011'1111'1111'1111'1111'1111ui32, range32[2]);
  //EXPECT_EQ(2, nextWordWithZeroBit);
  EXPECT_EQ(2 * 32 + 22, f2f::util::FindAndSetFirstZeroBit(range, nextWordWithZeroBit, WordCount, nextWordWithZeroBit));
  //EXPECT_EQ(3, nextWordWithZeroBit);
  EXPECT_EQ(0xFFFFFFFF, range32[2]);
  EXPECT_EQ(3 * 32, f2f::util::FindAndSetFirstZeroBit(range, nextWordWithZeroBit, WordCount, nextWordWithZeroBit));
  EXPECT_EQ(0xFFFFFFFF, range32[3]);
}

TEST(BitRange, FindAndSetFirstZeroBit)
{
  FindAndSetFirstZeroBitT<uint32_t>();
  FindAndSetFirstZeroBitT<uint64_t>();
}

TEST(BitRange, FindAndSetFirstZeroBit2)
{
  uint32_t range[] = { 16383 };
  int nextWordWithZeroBit;
  EXPECT_EQ(14, f2f::util::FindAndSetFirstZeroBit(range, 0, 1, nextWordWithZeroBit));
  EXPECT_EQ(0, nextWordWithZeroBit);
}

inline int FindLastSetBitWrapper(uint32_t const * range, unsigned bitsCount)
{
  int res32 = f2f::util::FindLastSetBit(range, bitsCount);
  int res64 = f2f::util::FindLastSetBit(reinterpret_cast<uint64_t const *>(range), bitsCount);
  EXPECT_EQ(res32, res64);
  return res32;
}

TEST(BitRange, FindLastSetBit)
{
  uint32_t const range1[] = { 1, 0 };
  EXPECT_EQ(0, f2f::util::FindLastSetBit(range1, 32*2));

  uint32_t const range2[] = { 0xFFFFFFFFui32, 0b1001'1111'1000'0000'0000'0011'1111'1111ui32 };
  EXPECT_EQ(63, FindLastSetBitWrapper(range2, 32 * 2));
  EXPECT_EQ(60, FindLastSetBitWrapper(range2, 32 + 31));
  EXPECT_EQ(55, FindLastSetBitWrapper(range2, 32 + 24));
  EXPECT_EQ(41, FindLastSetBitWrapper(range2, 32 + 23));
  EXPECT_EQ(31, FindLastSetBitWrapper(range2, 32));
  EXPECT_EQ(32, FindLastSetBitWrapper(range2, 33));
  EXPECT_EQ(30, FindLastSetBitWrapper(range2, 31));
  EXPECT_EQ(-1, FindLastSetBitWrapper(range2, 0));
  EXPECT_EQ(0,  FindLastSetBitWrapper(range2, 1));

  uint32_t const range3[] = { 0, 0 };
  EXPECT_EQ(-1, FindLastSetBitWrapper(range3, 32 * 2));
}