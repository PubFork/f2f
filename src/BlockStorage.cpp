#include "BlockStorage.hpp"
#include <limits>
#include <boost/multiprecision/integer.hpp>
#include "util/BitRange.hpp"
#include "util/StorageT.hpp"
#include "format/Common.hpp"
#include "Exception.hpp"

/*

TODO:
- Optimize free group search

*/

namespace f2f
{

uint64_t BlockAddress::absoluteAddress() const
{
  return 
    (m_blockIndex / format::OccupancyBlock::BitmapItemsCount + 1) * format::OccupancyBlockSize
    + m_blockIndex * format::AddressableBlockSize;
}

uint64_t BlockStorage::getOccupancyBlockPosition(uint64_t groupIndex)
{
  return groupIndex * (format::OccupancyBlockSize 
    + format::OccupancyBlock::BitmapItemsCount * format::AddressableBlockSize);
}

uint64_t BlockStorage::getBlockGroupIndex(uint64_t blockIndex)
{
  return blockIndex / format::OccupancyBlock::BitmapItemsCount;
}

unsigned BlockStorage::getBlockIndexInGroup(uint64_t blockIndex)
{
  return blockIndex % format::OccupancyBlock::BitmapItemsCount;
}

uint64_t BlockStorage::getSizeForNBlocks(uint64_t numBlocks)
{
  if (numBlocks == 0)
    return 0;
  return BlockAddress::fromBlockIndex(numBlocks - 1).absoluteAddress() + format::AddressableBlockSize;
}

uint64_t BlockStorage::groupsCount() const 
{ 
  if (m_blocksCount == 0)
    return 0;
  return getBlockGroupIndex(m_blocksCount - 1) + 1; 
}

BlockStorage::BlockStorage(IStorage & storage, bool format)
  : m_storage(storage)
{
  if (format)
  {
    m_blocksCount = 0;
    storage.resize(0);
  }
  else
  {
    static unsigned const WholeGroupSize = format::OccupancyBlockSize
      + format::OccupancyBlock::BitmapItemsCount * format::AddressableBlockSize;
    uint64_t lastOccupancyGroupSize = storage.size() % WholeGroupSize;
    if (storage.size() != 0
        && lastOccupancyGroupSize > 0
        && ( lastOccupancyGroupSize < format::OccupancyBlockSize
          || (lastOccupancyGroupSize - format::OccupancyBlockSize) % format::AddressableBlockSize != 0))
      F2F_THROW_INVALID_FORMAT("Incorrect file size");

    m_blocksCount = storage.size() / WholeGroupSize * format::OccupancyBlock::BitmapItemsCount;
    if (lastOccupancyGroupSize != 0)
      m_blocksCount += (lastOccupancyGroupSize - format::OccupancyBlockSize) / format::AddressableBlockSize;
  }
}

BlockAddress BlockStorage::allocateBlock()
{
  BlockAddress newBlock;
  allocateBlocks(1, [&newBlock](BlockAddress const & block) { newBlock = block; });
  return newBlock;
}

void BlockStorage::allocateBlocks(uint64_t numBlocks, std::function<void (BlockAddress const &)> const & visitor/*, locationHints*/)
{
  for(uint64_t groupIndex = 0, groupsN = groupsCount(); 
    numBlocks > 0 && groupIndex < groupsN; ++groupIndex)
  {
    bool blockIsDirty = false;
    format::OccupancyBlock block;
    util::readT(m_storage, getOccupancyBlockPosition(groupIndex), block);
    
    for(int nextBitmapWord = 0; numBlocks > 0 && nextBitmapWord != -1;)
    {
      int occupiedBlockInGroup = 
        util::FindAndSetFirstZeroBit(
          block.bitmap, nextBitmapWord, format::OccupancyBlock::BitmapWordsCount, nextBitmapWord);
      if (occupiedBlockInGroup == -1)
        break;

      blockIsDirty = true;

      uint64_t occupiedBlock = groupIndex * format::OccupancyBlock::BitmapItemsCount + occupiedBlockInGroup;

      // How it can be that unallocated block 'm_blocksCount' marked as occupied?
      F2F_FORMAT_ASSERT(occupiedBlock <= m_blocksCount);

      if (occupiedBlock == m_blocksCount)
        // We're in the last group, need to append blocks
        break;

      visitor(BlockAddress::fromBlockIndex(occupiedBlock));
      --numBlocks;
    }
    if (blockIsDirty)
      util::writeT(m_storage, getOccupancyBlockPosition(groupIndex), block);
  }
  if (numBlocks > 0)
    appendBlocks(numBlocks, visitor);
}

void BlockStorage::appendBlocks(uint64_t numBlocks, std::function<void(BlockAddress const &)> const & visitor)
{
  uint64_t originalBlocksCount = m_blocksCount;
  m_blocksCount += numBlocks;
  m_storage.resize(getSizeForNBlocks(m_blocksCount));
  markBlocks(originalBlocksCount, numBlocks, Status::occupied);
  for(uint64_t blockIndex = originalBlocksCount; blockIndex < m_blocksCount; ++blockIndex)
    visitor(BlockAddress::fromBlockIndex(blockIndex));
}

void BlockStorage::releaseBlocks(BlockAddress blockAddress, unsigned numBlocks)
{
  auto blockIndex = blockAddress.index();

  if (blockIndex + numBlocks > m_blocksCount)
    throw std::runtime_error("Expectation fault: Invalid argument");

  if (blockIndex + numBlocks == m_blocksCount)
  {
    // Truncate as much as possible including all free blocks at the end
    blockIndex = findStartOfFreeBlocksRange(blockIndex);
    truncateStorage(blockIndex);

    unsigned blockInGroup = getBlockIndexInGroup(blockIndex);
    if (blockInGroup != 0)
    {
      // If not whole group truncated
      uint64_t beginGroupIndex = getBlockGroupIndex(blockIndex);
      markBlocksInGroup(beginGroupIndex, blockInGroup, format::OccupancyBlock::BitmapItemsCount - 1, Status::free);
    }
  }
  else
    markBlocks(blockIndex, numBlocks, Status::free);
}

bool BlockStorage::isAdjacentBlocks(BlockAddress blockRangeStart, unsigned rangeSize, BlockAddress blockIndex2)
{
  auto lastBlockInRange = blockRangeStart.index() + rangeSize - 1;
  return lastBlockInRange + 1 == blockIndex2.index()
    && getBlockGroupIndex(lastBlockInRange) == getBlockGroupIndex(blockIndex2.index());
}

int64_t BlockStorage::findStartOfFreeBlocksRange(uint64_t endBlockIndex) const
{
  if (endBlockIndex == 0)
    return 0;
  uint64_t endGroupIndex = getBlockGroupIndex(endBlockIndex - 1);
  for (uint64_t groupIndex = endGroupIndex;; --groupIndex)
  {
    format::OccupancyBlock block;
    util::readT(m_storage, getOccupancyBlockPosition(groupIndex), block);

    unsigned lastBit = format::OccupancyBlock::BitmapItemsCount;
    if (groupIndex == endGroupIndex)
      lastBit = getBlockIndexInGroup(endBlockIndex - 1) + 1;

    int lastOccupiedBlockInGroup = util::FindLastSetBit(block.bitmap, lastBit);
    if (lastOccupiedBlockInGroup != -1)
      return groupIndex * format::OccupancyBlock::BitmapItemsCount + lastOccupiedBlockInGroup + 1;

    if (groupIndex == 0)
      break;
  }
  return 0;
}

void BlockStorage::markBlocks(uint64_t blockIndex, uint64_t numBlocks, Status status)
{
  uint64_t beginGroupIndex = getBlockGroupIndex(blockIndex);
  uint64_t endGroupIndex = getBlockGroupIndex(blockIndex + numBlocks - 1);
  for(uint64_t groupIndex = beginGroupIndex; groupIndex <= endGroupIndex; ++groupIndex)
  {
    unsigned beginBlock = 0;
    if (groupIndex == beginGroupIndex)
      beginBlock = getBlockIndexInGroup(blockIndex);
    unsigned endBlock = format::OccupancyBlock::BitmapItemsCount - 1;
    if (groupIndex == endGroupIndex)
      endBlock = getBlockIndexInGroup(blockIndex + numBlocks - 1);
    markBlocksInGroup(groupIndex, beginBlock, endBlock, status);
  }
}

void BlockStorage::markBlocksInGroup(uint64_t groupIndex, unsigned beginBlock, unsigned endBlock, Status status)
{
  format::OccupancyBlock block;
  util::readT(m_storage, getOccupancyBlockPosition(groupIndex), block);
  if (status == Status::free)
    util::ClearBitRange(block.bitmap, beginBlock, endBlock);
  else
    util::SetBitRange(block.bitmap, beginBlock, endBlock);
  util::writeT(m_storage, getOccupancyBlockPosition(groupIndex), block);
}

void BlockStorage::truncateStorage(uint64_t numBlocks)
{
  m_blocksCount = numBlocks;
  m_storage.resize(getSizeForNBlocks(m_blocksCount));
}

void BlockStorage::check() const
{
}

void BlockStorage::checkAllocatedBlock(BlockAddress blockIndex) const
{
  F2F_FORMAT_ASSERT(blockIndex.index() < m_blocksCount);

  format::OccupancyBlock block;
  util::readT(m_storage, getOccupancyBlockPosition(getBlockGroupIndex(blockIndex.index())), block);
  F2F_FORMAT_ASSERT(util::GetBitInRange(block.bitmap, getBlockIndexInGroup(blockIndex.index())));
}

void BlockStorage::enumerateAllocatedBlocks(std::function<void(BlockAddress const &)> const & visitor) const
{
  for (uint64_t groupIndex = 0, blockIndex = 0; blockIndex < m_blocksCount; ++groupIndex)
  {
    format::OccupancyBlock block;
    util::readT(m_storage, getOccupancyBlockPosition(groupIndex), block);
    for(unsigned blockInGroupIndex = 0; 
      blockIndex < m_blocksCount
        && blockInGroupIndex < format::OccupancyBlock::BitmapItemsCount; 
      ++blockIndex, ++blockInGroupIndex)
      if (util::GetBitInRange(block.bitmap, blockInGroupIndex))
        visitor(BlockAddress::fromBlockIndex(blockIndex));
  }
}

}