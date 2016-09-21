#pragma once

#include <cstdint>
#include <limits>
#include "Common.hpp"
#include "Inode.hpp"

namespace f2f { namespace format 
{

#pragma pack(push, 1)

struct DirectoryTreeLeafItem
{
  // DirectoryFlag is set in inode value for directories
  static const uint64_t DirectoryFlag = UINT64_C(1) << 63;

  uint64_t inode;
  uint32_t nameHash;
  uint16_t nameSize;
  char name[1];
};

struct DirectoryTreeLeafHeader
{
  // dataSize and MaxDataSize are sizes of sequence of DirectoryTreeLeafItem items (header with following string)
  static const unsigned MaxDataSize = AddressableBlockSize - 2 /*dataSize*/ - 8 /*nextLeafNode*/;
  static const uint64_t NoNextLeaf = std::numeric_limits<uint64_t>::max();

  uint16_t dataSize;
  uint64_t nextLeafNode;
  // Items are in sorted order
  DirectoryTreeLeafItem head;
};

struct DirectoryTreeLeaf: DirectoryTreeLeafHeader
{
  char itemsData[AddressableBlockSize - sizeof(DirectoryTreeLeafHeader)];
};

struct DirectoryTreeChildNodeReference
{
  uint64_t childBlockIndex;
  uint32_t nameHash;
};

struct DirectoryTreeInternalNode
{
  static const unsigned MaxCount = (AddressableBlockSize - 2) / sizeof(DirectoryTreeChildNodeReference);

  uint16_t itemsCount;
  DirectoryTreeChildNodeReference children[MaxCount];
};

static const unsigned MaxFileNameSize = DirectoryTreeLeafHeader::MaxDataSize
        - offsetof(DirectoryTreeLeafHeader, head) - offsetof(DirectoryTreeLeafItem, name);

struct DirectoryInode : InodeHeader
{
  uint64_t parentDirectoryInode;
  uint16_t levelsCount;
  static const unsigned PayloadSize = AddressableBlockSize - sizeof(InodeHeader) - 2 - 8;

  struct IndirectReferences
  {
    static const unsigned MaxCount = (PayloadSize - 2) / sizeof(DirectoryTreeChildNodeReference);

    uint16_t itemsCount;
    DirectoryTreeChildNodeReference children[MaxCount];
  };

  struct DirectReferences
  {
    static const unsigned MaxDataSize = PayloadSize - 2 /* dataSize */;

    // dataSize and MaxDataSize are sizes of sequence of DirectoryTreeLeafItem items with following strings
    uint16_t dataSize;
    DirectoryTreeLeafItem head;
    char itemsData[MaxDataSize - sizeof(DirectoryTreeLeafItem)];
  };

  union
  {
    IndirectReferences indirectReferences;
    DirectReferences directReferences;
  };
};

static_assert(sizeof(DirectoryInode) <= AddressableBlockSize, "");

#pragma pack(pop)

}}
