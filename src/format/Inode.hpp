#pragma once

#include <cstdint>
#include "Common.hpp"
#include "File.hpp"

namespace f2f { namespace format 
{

#pragma pack(push,1)

struct InodeHeader
{
  uint16_t flags;
  uint64_t fileSize;
  uint64_t blocksCount;
  char reservedForMetadata[32];
};

struct FileInode: InodeHeader
{
  uint16_t levelsCount;
  static const unsigned PayloadSize = AddressableBlockSize - sizeof(InodeHeader) - 2;

  struct IndirectReferences
  {
    static const unsigned MaxCount = 20;

    uint16_t itemsCount;
    ChildNodeReference children[MaxCount];
  };

  struct DirectReferences
  {
    static const unsigned MaxCount = 20;

    uint16_t itemsCount;
    BlockRange ranges[MaxCount];
  };

  union
  {
    DirectReferences directReferences;
    IndirectReferences indirectReferences;
  };
};

static_assert(sizeof(FileInode) <= AddressableBlockSize, "");

#pragma pack(pop)

}}