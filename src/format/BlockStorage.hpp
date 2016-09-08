#pragma once

#include <cstdint>

namespace f2f { namespace format 
{

static const int OccupancyBlockSize = 1024; // in bytes

struct OccupancyBlock
{
  typedef size_t BitmapWord; // TODO: deal with endianness
  static const int BitmapWordsCount = OccupancyBlockSize / sizeof(BitmapWord);
  static const int BitmapItemsCount = BitmapWordsCount * sizeof(BitmapWord) * 8;
  BitmapWord bitmap[BitmapWordsCount];
};

static_assert(OccupancyBlockSize == sizeof(OccupancyBlock), "");

}}