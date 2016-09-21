#pragma once

#include <boost/optional.hpp>
#include "f2f/IStorage.hpp"
#include "format/BlockStorage.hpp"
#include "format/StorageHeader.hpp"

namespace f2f
{

class BlockAddress
{
public:
  BlockAddress() = default;

  uint64_t absoluteAddress() const;
  inline uint64_t index() const { return m_blockIndex; }

  static BlockAddress fromBlockIndex(uint64_t blockIndex)
  {
    return BlockAddress(blockIndex);
  }

  bool operator==(BlockAddress const & rhs) const
  { 
    return m_blockIndex == rhs.m_blockIndex; 
  }

  bool operator<(BlockAddress const & rhs) const
  {
    return m_blockIndex < rhs.m_blockIndex;
  }

private:
  BlockAddress(uint64_t blockIndex): m_blockIndex(blockIndex) {}

  uint64_t m_blockIndex;
};

class BlockStorage
{
public:
  explicit BlockStorage(IStorage &, bool format = false);

  IStorage & storage() const { return m_storage; }

  BlockAddress allocateBlock();
  void allocateBlocks(uint64_t numBlocks, std::function<void (BlockAddress const &)> const & visitor);
  void releaseBlocks(BlockAddress blockIndex, unsigned numBlocks);
  static bool isAdjacentBlocks(BlockAddress blockRangeStart, unsigned rangeSize, BlockAddress blockIndex2);

  // Diagnostics
  void check() const;
  void checkAllocatedBlock(BlockAddress blockIndex) const;
  void enumerateAllocatedBlocks(std::function<void(BlockAddress const &)> const & visitor) const;
  uint64_t blocksCount() const { return m_blocksCount; }

private:
  IStorage & m_storage;
  uint64_t m_blocksCount;
  format::StorageHeader m_storageHeader;

  bool allocateBlocksLevel0(uint64_t & numBlocks,
    std::function<void(BlockAddress const &)> const & visitor,
    uint64_t absoluteOffset, uint64_t blocksOffset);
  bool allocateBlocks(uint64_t & numBlocks, 
    std::function<void(BlockAddress const &)> const & visitor,
    unsigned level, uint64_t absoluteOffset, uint64_t blocksOffset);

  void truncateStorage(uint64_t numBlocks);

  bool markBlocksAsFree(uint64_t beginBlockInGroup, uint64_t endBlockInGroup,
    unsigned level, uint64_t absoluteOffset, uint64_t blocksOffset);
  int64_t findStartOfFreeBlocksRange(uint64_t blockIndex) const;

  struct CheckState;
  bool checkLevel(CheckState &, unsigned level, uint64_t absoluteOffset, uint64_t blocksOffset) const;

  static uint64_t getOccupancyBlockPosition(uint64_t groupIndex);
  static uint64_t getBlockGroupIndex(uint64_t blockIndex);
  static unsigned getBlockIndexInGroup(uint64_t blockIndex);
  static uint64_t getSizeForNBlocks(uint64_t numBlocks);
  static uint64_t getBlocksCountByStorageSize(uint64_t size);
};

}