#pragma once

#include <cstdint>

namespace f2f { namespace format 
{

#pragma pack(push,1)

struct StorageHeader
{
  static const uint16_t MagicValue = 0xF2F0;

  uint16_t magic; 
  char reserved[6];
  uint64_t occupiedBlocksCount;
};

#pragma pack(pop)

}}