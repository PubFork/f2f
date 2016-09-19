#include "BlockStorage.hpp"
#include <limits>
#include <boost/multiprecision/integer.hpp>
#include "util/BitRange.hpp"
#include "util/StorageT.hpp"
#include "format/Common.hpp"
#include "Exception.hpp"

namespace f2f
{

namespace
{

struct OccupancyGroupLevelsInit
{
  static const unsigned LevelsCount = 4;
  OccupancyGroupLevelsInit()
  {
    levelAbsoluteSize[0] =
      format::OccupancyBlockSize + format::OccupancyBlock::BitmapItemsCount * format::AddressableBlockSize;
    blocksInLevel[0] = format::OccupancyBlock::BitmapItemsCount;
    for (int level = 1; level < LevelsCount; ++level)
    {
      levelAbsoluteSize[level] =
        levelAbsoluteSize[level - 1] * format::OccupancyBlock::BitmapItemsCount
        + format::OccupancyBlockSize;
      blocksInLevel[level] = blocksInLevel[level - 1] * format::OccupancyBlock::BitmapItemsCount;
    }
  }

  uint64_t levelAbsoluteSize[LevelsCount];
  uint64_t blocksInLevel[LevelsCount];
};

const OccupancyGroupLevelsInit OccupancyGroupLevels;

}

uint64_t BlockAddress::absoluteAddress() const
{
  uint64_t occupancyBlocks = m_blockIndex / format::OccupancyBlock::BitmapItemsCount + 1;
  for(int level = 1; level < OccupancyGroupLevels.LevelsCount; ++level)
  {
    occupancyBlocks += 
      (m_blockIndex + (OccupancyGroupLevels.blocksInLevel[level] - OccupancyGroupLevels.blocksInLevel[level - 1])) 
      / OccupancyGroupLevels.blocksInLevel[level];
  }
  return occupancyBlocks * format::OccupancyBlockSize
    + m_blockIndex * format::AddressableBlockSize;
}

uint64_t BlockStorage::getOccupancyBlockPosition(uint64_t groupIndex)
{
  return BlockAddress::fromBlockIndex(groupIndex * format::OccupancyBlock::BitmapItemsCount).absoluteAddress() 
    - format::OccupancyBlockSize;
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

uint64_t BlockStorage::getBlocksCountByStorageSize(uint64_t size)
{
  uint64_t blocksCount = 0;
  for(int level = OccupancyGroupLevels.LevelsCount - 1; level >= 0; --level)
  {
    uint64_t groupsCount = size / OccupancyGroupLevels.levelAbsoluteSize[level];
    size %= OccupancyGroupLevels.levelAbsoluteSize[level];
    blocksCount += groupsCount * OccupancyGroupLevels.blocksInLevel[level];
    if (level == 0)
    {
      if (size > 0
        && (size < format::OccupancyBlockSize
          || (size - format::OccupancyBlockSize) % format::AddressableBlockSize != 0))
        F2F_THROW_INVALID_FORMAT("Incorrect file size");
      if (size > 0)
        blocksCount += (size - format::OccupancyBlockSize) / format::AddressableBlockSize;
    }
    else
      if (size > OccupancyGroupLevels.levelAbsoluteSize[level - 1])
        size -= format::OccupancyBlockSize;
  }
  return blocksCount;
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
    m_blocksCount = getBlocksCountByStorageSize(storage.size());
  }
}

BlockAddress BlockStorage::allocateBlock()
{
  BlockAddress newBlock;
  allocateBlocks(1, [&newBlock](BlockAddress const & block) { newBlock = block; });
  return newBlock;
}

void BlockStorage::allocateBlocks(uint64_t numBlocks, std::function<void(BlockAddress const &)> const & visitor)
{
  allocateBlocks(numBlocks, visitor, OccupancyGroupLevels.LevelsCount - 1, 0, 0);
}

// Returns true if group still has free blocks
bool BlockStorage::allocateBlocks(uint64_t & numBlocks, 
  std::function<void(BlockAddress const &)> const & visitor,
  unsigned level, uint64_t absoluteOffset, uint64_t blocksOffset)
{
  if (level == 0)
  {
    return allocateBlocksLevel0(numBlocks, visitor, absoluteOffset, blocksOffset);
  }
  else
  {
    uint64_t position = absoluteOffset + OccupancyGroupLevels.levelAbsoluteSize[level - 1];
    bool blockIsDirty = false;
    format::OccupancyBlock block;
    if (position >= m_storage.size())
    {
      // Occupancy block of this level isn't created yet, but it may be created
      // during processing the loop
      memset(&block, 0, sizeof(block));
    }
    else
    {
      util::readT(m_storage, position, block);
    }
    int nextBitmapWord = 0;
    for (; numBlocks > 0 && nextBitmapWord != -1;)
    {
      int freeGroup = util::FindFirstZeroBit(
        block.bitmap, nextBitmapWord, format::OccupancyBlock::BitmapWordsCount, nextBitmapWord);
      if (freeGroup == -1)
        break;
      if (!allocateBlocks(numBlocks, visitor, level - 1, 
        freeGroup == 0 
          ? absoluteOffset 
          : (absoluteOffset + format::OccupancyBlockSize + freeGroup * OccupancyGroupLevels.levelAbsoluteSize[level - 1]),
        blocksOffset + freeGroup * OccupancyGroupLevels.blocksInLevel[level - 1]))
      {
        blockIsDirty = true;
        util::SetBit(block.bitmap, freeGroup);
      }
    }
    if (blockIsDirty)
      if (position < m_storage.size())
        util::writeT(m_storage, position, block);
    return nextBitmapWord != -1;
  }
}

// Returns true if group still has free blocks
bool BlockStorage::allocateBlocksLevel0(uint64_t & numBlocks, 
  std::function<void(BlockAddress const &)> const & visitor,
  uint64_t absoluteOffset, uint64_t blocksOffset)
{
  bool blockIsDirty = false;
  format::OccupancyBlock block;
  if (absoluteOffset >= m_storage.size())
  {
    // Occupancy block of this level isn't created yet, but it may be created
    // during processing the loop
    memset(&block, 0, sizeof(block));
  }
  else
    util::readT(m_storage, absoluteOffset, block);

  int nextBitmapWord = 0;
  for (; numBlocks > 0 && nextBitmapWord != -1;)
  {
    int occupiedBlockInGroup =
      util::FindAndSetFirstZeroBit(
        block.bitmap, nextBitmapWord, format::OccupancyBlock::BitmapWordsCount, nextBitmapWord);
    if (occupiedBlockInGroup == -1)
      break;

    blockIsDirty = true;

    uint64_t occupiedBlock = blocksOffset + occupiedBlockInGroup;

    // How it can be that unallocated block 'm_blocksCount' marked as occupied?
    F2F_FORMAT_ASSERT(occupiedBlock <= m_blocksCount);

    if (occupiedBlock == m_blocksCount)
    {
      // We're in the last group, need to append blocks
      m_storage.resize(getSizeForNBlocks(m_blocksCount + numBlocks));
      m_blocksCount += numBlocks;
    }

    visitor(BlockAddress::fromBlockIndex(occupiedBlock));
    --numBlocks;
  }
  if (blockIsDirty)
    util::writeT(m_storage, absoluteOffset, block);

  return nextBitmapWord != -1;
}

// Returns true if group was fully occupied 
bool BlockStorage::markBlocksAsFree(uint64_t beginBlockInGroup, uint64_t endBlockInGroup,
  unsigned level, uint64_t absoluteOffset, uint64_t blocksOffset)
{
  if (level == 0)
  {
    assert(beginBlockInGroup <= endBlockInGroup);
    assert(endBlockInGroup < format::OccupancyBlock::BitmapItemsCount);
    if (absoluteOffset < m_storage.size())
    {
      format::OccupancyBlock block;
      util::readT(m_storage, absoluteOffset, block);
      bool hadFreeBlocks = util::HasZeroBit(block.bitmap, format::OccupancyBlock::BitmapWordsCount);
      util::ClearBitRange(block.bitmap, beginBlockInGroup, endBlockInGroup);
      util::writeT(m_storage, absoluteOffset, block);
      return !hadFreeBlocks;
    }
    else
      return false; // return value "didn't have free blocks" isn't completely true, but it should work
  }
  else
  {
    bool blockIsDirty = false;
    uint64_t beginSubGroup = beginBlockInGroup / OccupancyGroupLevels.blocksInLevel[level - 1];
    uint64_t endSubGroup = endBlockInGroup / OccupancyGroupLevels.blocksInLevel[level - 1];
    assert(beginSubGroup <= endSubGroup);
    assert(endSubGroup < format::OccupancyBlock::BitmapItemsCount);
    for(uint64_t subGroup = beginSubGroup; subGroup <= endSubGroup; ++subGroup)
    {
      uint64_t beginBlockInSubGroup = 0;
      uint64_t endBlockInSubGroup = OccupancyGroupLevels.blocksInLevel[level - 1] - 1;
      if (subGroup == beginSubGroup)
        beginBlockInSubGroup = beginBlockInGroup % OccupancyGroupLevels.blocksInLevel[level - 1];
      if (subGroup == endSubGroup)
        endBlockInSubGroup = endBlockInGroup % OccupancyGroupLevels.blocksInLevel[level - 1];
      if (markBlocksAsFree(beginBlockInSubGroup, endBlockInSubGroup, level - 1,
          subGroup == 0
            ? absoluteOffset
            : (absoluteOffset + format::OccupancyBlockSize + subGroup * OccupancyGroupLevels.levelAbsoluteSize[level - 1]),
          blocksOffset + subGroup * OccupancyGroupLevels.blocksInLevel[level - 1]))
        blockIsDirty = true;
    }

    bool hadFreeBlocks = true;
    uint64_t position = absoluteOffset + OccupancyGroupLevels.levelAbsoluteSize[level - 1];
    if (position < m_storage.size())
    {
      format::OccupancyBlock block;
      util::readT(m_storage, position, block);
      bool hadFreeBlocks = util::HasZeroBit(block.bitmap, format::OccupancyBlock::BitmapWordsCount);
      util::ClearBitRange(block.bitmap, beginSubGroup, endSubGroup);
      util::writeT(m_storage, position, block);
    }
    return !hadFreeBlocks;
  }
}

void BlockStorage::releaseBlocks(BlockAddress blockAddress, unsigned numBlocks)
{
  auto blockIndex = blockAddress.index();

  if (blockIndex + numBlocks > m_blocksCount)
    throw std::runtime_error("Expectation fault: Invalid argument");

  uint64_t endBlockIndex;
  if (blockIndex + numBlocks == m_blocksCount)
  {
    // Truncate as much as possible including all free blocks at the end
    blockIndex = findStartOfFreeBlocksRange(blockIndex);
    endBlockIndex = m_blocksCount - 1;
    truncateStorage(blockIndex);
  }
  else
    endBlockIndex = blockIndex + numBlocks - 1;

  markBlocksAsFree(blockIndex, endBlockIndex, OccupancyGroupLevels.LevelsCount - 1, 0, 0);
}

bool BlockStorage::isAdjacentBlocks(BlockAddress blockRangeStart, unsigned rangeSize, BlockAddress blockIndex2)
{
  auto lastBlockInRange = blockRangeStart.index() + rangeSize - 1;
  return lastBlockInRange + 1 == blockIndex2.index()
    && BlockAddress::fromBlockIndex(lastBlockInRange).absoluteAddress() 
      + format::AddressableBlockSize == blockIndex2.absoluteAddress();
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