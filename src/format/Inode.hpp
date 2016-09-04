#pragma once

#include <cstdint>
#include "Common.hpp"

namespace f2f { namespace format {

#pragma pack(push,1)

struct BlockRange
{
  static const unsigned MaxCount = 0xffff;

  uint32_t blockIndexLo;
  uint16_t blockIndexHi;
  uint16_t blocksCount;
  uint64_t fileOffset;

  uint64_t blockIndex() const { return blockIndexLo + (uint64_t(blockIndexHi) << 16); }
  void setBlockIndex(uint64_t index) { blockIndexLo = uint32_t(index); blockIndexHi = uint16_t(index >> 32); }
};

struct ChildNodeReference
{
  uint64_t childBlockIndex;
  uint64_t fileOffset;
};

struct InodeHeader
{
  static const uint16_t FlagIndirectRanges = 0x100;
  static const uint16_t FlagDirty = 0x200; // Runtime-only flag

  uint16_t flags;
  uint64_t fileSize;
  uint64_t blocksCount;
  uint64_t creationTime;
  
  static const int DirectRangesSize = 10;
  uint16_t rangesCount; // = DirectRangesSize when tree is used
  BlockRange directRanges[DirectRangesSize];

  static const uint64_t NoRootBlock = std::numeric_limits<uint64_t>::max();
  uint64_t treeRootBlockIndex;
};

struct Inode: InodeHeader
{
  static const int PayloadSize = AddressableBlockSize - sizeof(InodeHeader);
  char payload[PayloadSize];
};

static_assert(sizeof(Inode) == AddressableBlockSize, "");

static const unsigned ChildReferencesSize = (AddressableBlockSize - 4) / sizeof(ChildNodeReference);
static const unsigned DirectRangesSize = ChildReferencesSize - 1;

struct BlockRangesInternalNode
{
  static const unsigned MaxCount = 
    (AddressableBlockSize - 4 /* isLeafNode + itemsCount */) / sizeof(ChildNodeReference);

  uint16_t itemsCount;
  ChildNodeReference children[MaxCount];
};

struct BlockRangesLeafNode
{
  static const unsigned MaxCount = BlockRangesInternalNode::MaxCount - 1;
  static const uint64_t NoNextLeaf = std::numeric_limits<uint64_t>::max();

  uint16_t itemsCount;
  uint64_t nextLeafNode;
  BlockRange ranges[MaxCount];
};

struct BlockRangesNode
{
  uint16_t isLeafNode;

  union
  {
    BlockRangesLeafNode leaf;
    BlockRangesInternalNode internal;
  };
};

#pragma pack(pop)

static_assert(sizeof(BlockRangesNode) <= AddressableBlockSize, "");

}}