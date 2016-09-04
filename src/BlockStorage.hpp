#pragma once

#include <boost/optional.hpp>
#include "IStorage.hpp"
#include "Format/BlockStorage.hpp"

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

private:
  BlockAddress(uint64_t blockIndex): m_blockIndex(blockIndex) {}

  uint64_t m_blockIndex;
};

class BlockStorage
{
public:
  explicit BlockStorage(IStorage &, bool format = false);

  IStorage & storage() const { return m_storage; }

  // TODO: return ranges instead?
  void allocateBlocks(uint64_t numBlocks, std::function<void (BlockAddress const &)> const & visitor);
  void releaseBlocks(uint64_t blockIndex, unsigned numBlocks);
  static bool isAdjacentBlocks(uint64_t blockIndex1, uint64_t blockIndex2);

  // Diagnostics
  void check() const;
  void checkAllocatedBlock(uint64_t blockIndex) const;
  void enumerateAllocatedBlocks(std::function<void(BlockAddress const &)> const & visitor) const;

private:
  IStorage & m_storage;
  uint64_t m_blocksCount;

  uint64_t groupsCount() const;

  void appendBlocks(uint64_t numBlocks, std::function<void(BlockAddress const &)> const & visitor);
  void truncateStorage(uint64_t numBlocks);

  enum class Status { occupied, free };
  void markBlocks(uint64_t blockIndex, uint64_t numBlocks, Status status);
  void markBlocksInGroup(uint64_t groupIndex, unsigned beginBlock, unsigned endBlock, Status status);
  int64_t findStartOfFreeBlocksRange(uint64_t blockIndex) const;

  static uint64_t getOccupancyBlockPosition(uint64_t groupIndex);
  static uint64_t getBlockGroupIndex(uint64_t blockIndex);
  static unsigned getBlockIndexInGroup(uint64_t blockIndex);
  static uint64_t getSizeForNBlocks(uint64_t numBlocks);
};

}