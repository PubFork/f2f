#include "FileBlocks.hpp"
#include "util/StorageT.hpp"
#include "util/FloorDiv.hpp"
#include "Exception.hpp"

namespace f2f
{

FileBlocks::FileBlocks(
  BlockStorage & blockStorage, 
  uint64_t & treeRootBlockIndex,
  bool & treeRootBlockIsDirty,
  bool initializeInode)
  : m_blockStorage(blockStorage)
  , m_storage(blockStorage.storage())
  , m_treeRootBlockIndex(treeRootBlockIndex)
  , m_treeRootBlockIsDirty(treeRootBlockIsDirty)
{
  if (initializeInode)
  {
    m_treeRootBlockIndex = format::Inode::NoRootBlock;
    m_treeRootBlockIsDirty = true;
  }
  else
    m_treeRootBlockIsDirty = false;
}

FileBlocks::OffsetAndSize const & FileBlocks::currentRange() const
{ 
  if (!m_position)
    throw std::runtime_error("Expectation fail: seek must have been called");
  return m_position->range;
}

void FileBlocks::moveToNextRange()
{
  if (!m_position)
    throw std::runtime_error("Expectation fail: seek must have been called");

  if (m_position->indexInBlock < m_position->block.itemsCount)
    ++m_position->indexInBlock;

  if (m_position->indexInBlock == m_position->block.itemsCount)
    if (m_position->block.nextLeafNode != format::BlockRangesLeafNode::NoNextLeaf)
    {
      format::BlockRangesNode blockRanges;
      util::readT(m_storage, BlockAddress::fromBlockIndex(m_position->block.nextLeafNode).absoluteAddress(), blockRanges);
      m_position->block = blockRanges.leaf;
      m_position->indexInBlock = 0;
      m_position->range = OffsetAndSize(blockRanges.leaf.ranges[0].blockIndex(), blockRanges.leaf.ranges[0].blocksCount);
    }
}

bool FileBlocks::eof() const
{
  if (!m_position)
    throw std::runtime_error("Expectation fail: seek must have been called");

  return m_position->indexInBlock == m_position->block.itemsCount;
}

void FileBlocks::seek(uint64_t blockIndex)
{
  if (m_position && m_position->block.ranges[0].fileOffset <= blockIndex 
    && blockIndex < m_position->block.ranges[m_position->block.itemsCount - 1].fileOffset + m_position->block.ranges[m_position->block.itemsCount - 1].blocksCount)
  {
    seekInLeaf(blockIndex, m_position->block);
  }
  else
  {
    seekTree(blockIndex, m_treeRootBlockIndex);
  }
}

void FileBlocks::seekInLeaf(uint64_t keyBlockIndex, format::BlockRangesLeafNode const & leaf)
{
  auto position = std::lower_bound(
    leaf.ranges,
    leaf.ranges + leaf.itemsCount,
    keyBlockIndex,
    [](format::BlockRange const & range, uint64_t blockIndex) -> bool {
    return range.fileOffset < blockIndex;
  }
  );
  if (position == leaf.ranges + leaf.itemsCount
    || position->fileOffset > keyBlockIndex)
    --position;
  if (position->fileOffset > keyBlockIndex || keyBlockIndex >= position->fileOffset + position->blocksCount)
    throw std::runtime_error("Expectation fail: node not found");

  if (!m_position || &m_position->block != &leaf)
  {
    m_position = Position();
    m_position->block = leaf;
  }
  m_position->indexInBlock = position - leaf.ranges;
  m_position->range = OffsetAndSize(
    position->blockIndex() + (keyBlockIndex - position->fileOffset),
    position->blocksCount - (keyBlockIndex - position->fileOffset));
}

void FileBlocks::seekTree(uint64_t keyBlockIndex, uint64_t nodeBlock) 
{
  format::BlockRangesNode blockRanges;
  util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), blockRanges);

  if (blockRanges.isLeafNode)
  {
    seekInLeaf(keyBlockIndex, blockRanges.leaf);
  }
  else
  {
    format::BlockRangesInternalNode const & internal = blockRanges.internal;
    auto position = std::lower_bound(
      internal.children + 1,
      internal.children + internal.itemsCount,
      keyBlockIndex,
      [](format::ChildNodeReference const & range, uint64_t blockIndex) -> bool {
        return range.fileOffset < blockIndex;
      }
    );
    if (position == internal.children + internal.itemsCount || position->fileOffset != keyBlockIndex)
      --position;
    seekTree(keyBlockIndex, position->childBlockIndex);
  }
}

void FileBlocks::append(uint64_t numBlocks)
{
  std::vector<format::ChildNodeReference> childrenToAdd;
  if (m_treeRootBlockIndex == format::Inode::NoRootBlock)
    childrenToAdd = appendToTreeLeaf(numBlocks);
  else
    childrenToAdd = appendToTree(numBlocks, m_treeRootBlockIndex);

  if (m_treeRootBlockIndex != format::Inode::NoRootBlock
    && !childrenToAdd.empty())
  {
    format::ChildNodeReference prevRootChildReference;
    prevRootChildReference.childBlockIndex = m_treeRootBlockIndex;
    prevRootChildReference.fileOffset = 0;
    childrenToAdd.insert(childrenToAdd.begin(), prevRootChildReference);
  }

  while (childrenToAdd.size() > 1)
    childrenToAdd = createInternalNodes(childrenToAdd.data(), childrenToAdd.data() + childrenToAdd.size());
  if (!childrenToAdd.empty())
  {
    m_treeRootBlockIndex = childrenToAdd.front().childBlockIndex;
    m_treeRootBlockIsDirty = true;
  }
  m_position.reset();
}

std::vector<format::ChildNodeReference> FileBlocks::appendToTreeLeaf(
  uint64_t numBlocks, format::BlockRangesLeafNode * node, bool * isDirty)
{
  std::vector<OffsetAndSize> newBlockRanges;
  m_blockStorage.allocateBlocks(numBlocks,
    [&newBlockRanges, this](BlockAddress block) {
    if (!newBlockRanges.empty()
      && m_blockStorage.isAdjacentBlocks(
        newBlockRanges.back().first + newBlockRanges.back().second - 1, block.absoluteAddress())
      && newBlockRanges.back().second < format::BlockRange::MaxCount)
      // Append to existing range
      ++newBlockRanges.back().second;
    else
      newBlockRanges.push_back(OffsetAndSize(block.index(), 1));
  });

  auto newBlocksStart = newBlockRanges.begin();
  uint64_t positonInFile = 0;

  if (node != nullptr)
  {
    *isDirty = !newBlockRanges.empty();

    auto & lastBlock = node->ranges[node->itemsCount - 1];
    // Try to join first new range with last existing
    if (
      m_blockStorage.isAdjacentBlocks(
        lastBlock.blockIndex() + lastBlock.blocksCount - 1, newBlocksStart->first)
      && lastBlock.blocksCount + newBlocksStart->second <= format::BlockRange::MaxCount)
    {
      lastBlock.blocksCount += newBlocksStart->second;
      ++newBlocksStart;
    }
    positonInFile = lastBlock.fileOffset + lastBlock.blocksCount;

    // Fill remaining free places in this block
    for (; newBlocksStart != newBlockRanges.end()
      && node->itemsCount < node->MaxCount;
      ++newBlocksStart, ++node->itemsCount)
    {
      auto & item = node->ranges[node->itemsCount];
      item.setBlockIndex(newBlocksStart->first);
      item.blocksCount = newBlocksStart->second;
      item.fileOffset = positonInFile;
      positonInFile += item.blocksCount;
    }
  }

  std::vector<format::ChildNodeReference> newSiblingReferences;

  // Create new blocks
  if (newBlocksStart != newBlockRanges.end())
  {
    unsigned blocksToAllocate =
      util::FloorDiv(newBlockRanges.end() - newBlocksStart, format::BlockRangesLeafNode::MaxCount);
    std::vector<BlockAddress> newLeafs;
    m_blockStorage.allocateBlocks(blocksToAllocate, [&newLeafs](BlockAddress block) {
      newLeafs.push_back(block);
    });
    newSiblingReferences.reserve(blocksToAllocate);
    if (node)
      node->nextLeafNode = newLeafs.front().index();
    for (auto newLeafIndexIt = newLeafs.begin(); newLeafIndexIt != newLeafs.end(); ++newLeafIndexIt)
    {
      auto const & newLeafIndex = *newLeafIndexIt;

      newSiblingReferences.push_back(format::ChildNodeReference());
      newSiblingReferences.back().childBlockIndex = newLeafIndex.index();
      newSiblingReferences.back().fileOffset = positonInFile;

      format::BlockRangesNode newNode;
      newNode.isLeafNode = true;
      format::BlockRangesLeafNode & newLeaf = newNode.leaf;

      // Init leaf forward reference
      if (newLeafIndexIt + 1 != newLeafs.end())
        newLeaf.nextLeafNode = (newLeafIndexIt + 1)->index();
      else
        newLeaf.nextLeafNode = newLeaf.NoNextLeaf;

      for (newLeaf.itemsCount = 0;
        newLeaf.itemsCount < newLeaf.MaxCount
        && newBlocksStart != newBlockRanges.end();
        ++newLeaf.itemsCount, ++newBlocksStart)
      {
        auto & item = newLeaf.ranges[newLeaf.itemsCount];
        item.setBlockIndex(newBlocksStart->first);
        item.blocksCount = newBlocksStart->second;
        item.fileOffset = positonInFile;
        positonInFile += item.blocksCount;
      }
      util::writeT(m_storage, newLeafIndex.absoluteAddress(), newNode);
    }
  }
  return newSiblingReferences;
}

std::vector<format::ChildNodeReference> FileBlocks::appendToTreeInternal(
  uint64_t numBlocks, format::BlockRangesInternalNode & node, bool & isDirty)
{
  auto newChildrenReferences = appendToTree(numBlocks, node.children[node.itemsCount - 1].childBlockIndex);
  auto newChildrenStart = newChildrenReferences.begin();

  // Fill remaining free places in this block
  for (; newChildrenStart != newChildrenReferences.end()
    && node.itemsCount < node.MaxCount;
    ++newChildrenStart, ++node.itemsCount)
  {
    node.children[node.itemsCount] = *newChildrenStart;
    isDirty = true;
  }

  if (newChildrenStart != newChildrenReferences.end())
  {
    // Create new blocks
    return createInternalNodes(&*newChildrenStart, &*newChildrenStart + (newChildrenReferences.end() - newChildrenStart));
  }
  return {};
}

std::vector<format::ChildNodeReference> FileBlocks::createInternalNodes(
  format::ChildNodeReference const * newChildrenStart, format::ChildNodeReference const * newChildrenEnd)
{
  std::vector<format::ChildNodeReference> newSiblingReferences;
  
  unsigned blocksToAllocate =
    util::FloorDiv(newChildrenEnd - newChildrenStart, format::BlockRangesInternalNode::MaxCount);
  std::vector<BlockAddress> newNodes;
  m_blockStorage.allocateBlocks(blocksToAllocate, [&newNodes](BlockAddress block) {
    newNodes.push_back(block);
  });
  newSiblingReferences.reserve(blocksToAllocate);
  for (auto const & newNodeIndex : newNodes)
  {
    newSiblingReferences.push_back(format::ChildNodeReference());
    newSiblingReferences.back().childBlockIndex = newNodeIndex.index();
    newSiblingReferences.back().fileOffset = newChildrenStart->fileOffset;

    format::BlockRangesNode newNode;
    newNode.isLeafNode = false;
    format::BlockRangesInternalNode & newInternal = newNode.internal;
    for (newInternal.itemsCount = 0;
      newInternal.itemsCount < newInternal.MaxCount
      && newChildrenStart != newChildrenEnd;
      ++newInternal.itemsCount, ++newChildrenStart)
    {
      newInternal.children[newInternal.itemsCount] = *newChildrenStart;
    }
    util::writeT(m_storage, newNodeIndex.absoluteAddress(), newNode);
  }
  return newSiblingReferences;
}

std::vector<format::ChildNodeReference> FileBlocks::appendToTree(uint64_t numBlocks, uint64_t nodeBlock)
{
  format::BlockRangesNode blockRanges;
  util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), blockRanges);

  std::vector<format::ChildNodeReference> result;
  bool isDirty = false;
  if (blockRanges.isLeafNode)
  {
    result = appendToTreeLeaf(numBlocks, &blockRanges.leaf, &isDirty);
  }
  else
  {
    result = appendToTreeInternal(numBlocks, blockRanges.internal, isDirty);
  }
  if (isDirty)
    util::writeT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), blockRanges);
  return result;
}

void FileBlocks::truncateTreeLeaf(uint64_t newSizeInBlocks, format::BlockRangesLeafNode & node, bool & isDirty)
{
  if (node.nextLeafNode != node.NoNextLeaf)
  {
    node.nextLeafNode = node.NoNextLeaf;
    isDirty = true;
  }

  for(; node.itemsCount > 0; --node.itemsCount)
  {
    auto & range = node.ranges[node.itemsCount - 1];

    if (range.fileOffset >= newSizeInBlocks)
    {
      m_blockStorage.releaseBlocks(range.blockIndex(), range.blocksCount);
      isDirty = true;
    }
    else
    {
      if (range.fileOffset + range.blocksCount > newSizeInBlocks)
      {
        uint16_t newBlocksCount = static_cast<uint16_t>(newSizeInBlocks - range.fileOffset);
        m_blockStorage.releaseBlocks(range.blockIndex() + newBlocksCount, range.blocksCount - newBlocksCount);
        range.blocksCount = newBlocksCount;
        isDirty = true;
      }
      break;
    }
  }
}

void FileBlocks::truncateTreeInternal(uint64_t newSizeInBlocks, format::BlockRangesInternalNode & node, bool & isDirty, uint64_t * newRoot)
{
  for (; node.itemsCount > 0; --node.itemsCount)
  {
    if (!truncateTree(
        newSizeInBlocks, 
        node.children[node.itemsCount - 1].childBlockIndex,
        node.itemsCount == 1 ? newRoot : nullptr)
      )
      break;
    isDirty = true;
  }
}

// Return true if node was deleted
bool FileBlocks::truncateTree(uint64_t newSizeInBlocks, uint64_t nodeBlock, uint64_t * newRoot)
{
  format::BlockRangesNode blockRanges;
  util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), blockRanges);

  bool isDirty = false;
  if (blockRanges.isLeafNode)
  {
    truncateTreeLeaf(newSizeInBlocks, blockRanges.leaf, isDirty);
    if (blockRanges.leaf.itemsCount == 0)
    {
      m_blockStorage.releaseBlocks(nodeBlock, 1);
      return true;
    }
    if (newRoot)
      *newRoot = nodeBlock;
  }
  else
  {
    if (newRoot)
      *newRoot = nodeBlock;
    truncateTreeInternal(newSizeInBlocks, blockRanges.internal, isDirty, newRoot);
    if (blockRanges.internal.itemsCount == 0)
    {
      m_blockStorage.releaseBlocks(nodeBlock, 1);
      return true;
    }
    else if (newRoot && *newRoot != nodeBlock)
    {
      m_blockStorage.releaseBlocks(nodeBlock, 1);
      return false;
    }
  }
  if (isDirty)
    util::writeT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), blockRanges);
  return false;
}

void FileBlocks::truncate(uint64_t newSizeInBlocks)
{
  auto originalRoot = m_treeRootBlockIndex;
  if (truncateTree(newSizeInBlocks, m_treeRootBlockIndex, &m_treeRootBlockIndex))
    m_treeRootBlockIndex = format::Inode::NoRootBlock;
  if (originalRoot != m_treeRootBlockIndex)
    m_treeRootBlockIsDirty = true;
  m_position.reset();
}

void FileBlocks::check() const
{
  if (m_treeRootBlockIndex != format::Inode::NoRootBlock)
  {
    CheckState state;
    state.filePosition = 0;
    state.level = 0;
    checkTree(m_treeRootBlockIndex, state);
    F2F_FORMAT_ASSERT(*state.lastNextLeafNodeReference == format::BlockRangesLeafNode::NoNextLeaf);
  }
}

void FileBlocks::checkTree(uint64_t nodeBlock, CheckState & state) const
{
  F2F_FORMAT_ASSERT(state.referencedBlocks.insert(nodeBlock).second);
  m_blockStorage.checkAllocatedBlock(nodeBlock);

  format::BlockRangesNode blockRanges;
  util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), blockRanges);

  if (blockRanges.isLeafNode)
  {
    if (!state.leafLevel)
      state.leafLevel = state.level;
    else
      F2F_FORMAT_ASSERT(state.leafLevel == state.level);

    auto const & leaf = blockRanges.leaf;

    if (state.lastNextLeafNodeReference)
      F2F_FORMAT_ASSERT(nodeBlock == *state.lastNextLeafNodeReference);
    state.lastNextLeafNodeReference = leaf.nextLeafNode;

    F2F_FORMAT_ASSERT(leaf.itemsCount > 0 && leaf.itemsCount <= leaf.MaxCount);
    for(int i = 0; i < leaf.itemsCount; ++i)
    {
      auto const & range = leaf.ranges[i];
      F2F_FORMAT_ASSERT(range.fileOffset == state.filePosition);
      state.filePosition += range.blocksCount;
      for(unsigned block = 0; block < range.blocksCount; ++block)
      {
        F2F_FORMAT_ASSERT(state.referencedBlocks.insert(range.blockIndex() + block).second);
        m_blockStorage.checkAllocatedBlock(range.blockIndex() + block);
      }
    }
  }
  else
  {
    auto const & internal = blockRanges.internal;
    F2F_FORMAT_ASSERT(internal.itemsCount > 0 && internal.itemsCount <= internal.MaxCount);

    for(int i = 0; i < internal.itemsCount; ++i)
    {
      auto const & child = internal.children[i];

      F2F_FORMAT_ASSERT(child.fileOffset == state.filePosition);
      ++state.level;
      checkTree(child.childBlockIndex, state);
      --state.level;
    }
  }
}

}