#pragma once

#include <boost/detail/endian.hpp>
#include <boost/multiprecision/integer.hpp>

namespace f2f { namespace util {

static_assert(BOOST_BYTE_ORDER == 1234, 
  "Following functions include optimizations for little-endian machines."
  " Their versions for other endianness are to be implemented.");

// [beginPosition, endPosition] - closed range
template<class BitmapWord>
void ClearBitRange(BitmapWord * bits, unsigned beginPosition, unsigned endPosition)
{
  static_assert(std::is_unsigned<BitmapWord>::value, "");

  unsigned beginBit = beginPosition % (sizeof(BitmapWord) * 8);
  unsigned endBit = endPosition % (sizeof(BitmapWord) * 8);
  unsigned beginWord = beginPosition / (sizeof(BitmapWord) * 8);
  unsigned endWord = endPosition / (sizeof(BitmapWord) * 8);
  BitmapWord beginMask = ~(std::numeric_limits<BitmapWord>::max() << beginBit);
  BitmapWord endMask = 0;
  if (endBit != sizeof(BitmapWord) * 8 - 1)
    endMask = std::numeric_limits<BitmapWord>::max() << (endBit + 1);
  if (beginWord == endWord)
  {
    bits[beginWord] &= beginMask | endMask;
  }
  else
  {
    bits[beginWord] &= beginMask;
    bits[endWord] &= endMask;
    for(unsigned word = beginWord + 1; word < endWord; ++word)
      bits[word] = 0;
  }
}

// [beginPosition, endPosition] - closed range
template<class BitmapWord>
void SetBitRange(BitmapWord * bits, unsigned beginPosition, unsigned endPosition)
{
  static_assert(std::is_unsigned<BitmapWord>::value, "");

  unsigned beginBit = beginPosition % (sizeof(BitmapWord) * 8);
  unsigned endBit = endPosition % (sizeof(BitmapWord) * 8);
  unsigned beginWord = beginPosition / (sizeof(BitmapWord) * 8);
  unsigned endWord = endPosition / (sizeof(BitmapWord) * 8);
  BitmapWord beginMask = std::numeric_limits<BitmapWord>::max() << beginBit;
  BitmapWord endMask = std::numeric_limits<BitmapWord>::max();
  if (endBit != sizeof(BitmapWord) * 8 - 1)
    endMask = ~(std::numeric_limits<BitmapWord>::max() << (endBit + 1));
  if (beginWord == endWord)
  {
    bits[beginWord] |= beginMask & endMask;
  }
  else
  {
    bits[beginWord] |= beginMask;
    bits[endWord] |= endMask;
    for (unsigned word = beginWord + 1; word < endWord; ++word)
      bits[word] = std::numeric_limits<BitmapWord>::max();
  }
}

template<class BitmapWord>
int FindAndSetFirstZeroBit(BitmapWord * bits, unsigned startWord, unsigned wordCount, int & nextWordWithZeroBit)
{
  static_assert(std::is_unsigned<BitmapWord>::value, "");

  nextWordWithZeroBit = -1;
  for (unsigned i = startWord; i < wordCount; ++i)
  {
    if (bits[i] != std::numeric_limits<BitmapWord>::max())
    {
      unsigned const firstZeroBit = boost::multiprecision::lsb(~bits[i]);
      unsigned const index = i * sizeof(BitmapWord) * 8 + firstZeroBit;
      bits[i] |= BitmapWord(1) << firstZeroBit;
      for (; i < wordCount; ++i)
        if (bits[i] != std::numeric_limits<BitmapWord>::max())
        {
          nextWordWithZeroBit = i;
          break;
        }

      return index;
    }
  }
  return -1;
}

template<class BitmapWord>
int FindLastSetBit(BitmapWord const * bits, unsigned bitsCount)
{
  static_assert(std::is_unsigned<BitmapWord>::value, "");

  if (bitsCount == 0)
    return -1;

  int lastWordIndex = (bitsCount - 1) / (sizeof(BitmapWord) * 8);
  for(int wordIndex = lastWordIndex; wordIndex >= 0; --wordIndex)
  {
    BitmapWord word = bits[wordIndex];

    if (wordIndex == lastWordIndex)
    {
      unsigned bitInWord = (bitsCount - 1) % (sizeof(BitmapWord) * 8);
      BitmapWord mask = std::numeric_limits<BitmapWord>::max();
      if (bitInWord != sizeof(BitmapWord) * 8 - 1)
        mask = ~(std::numeric_limits<BitmapWord>::max() << (bitInWord + 1));
      word &= mask;
    }

    if (word != 0)
      return wordIndex * sizeof(BitmapWord) * 8 + boost::multiprecision::msb(word);
  }
  return -1;
}

template<class BitmapWord>
bool GetBitInRange(BitmapWord const * bits, unsigned position)
{
  return (bits[position / (sizeof(BitmapWord) * 8)] >> (position % (sizeof(BitmapWord) * 8))) & 1;
}

}}