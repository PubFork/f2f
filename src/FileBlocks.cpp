#include "FileBlocks.hpp"
#include "util/StorageT.hpp"
#include "util/FloorDiv.hpp"
#include "Exception.hpp"

namespace f2f
{

FileBlocks::FileBlocks(
  BlockStorage & blockStorage, 
  format::FileInode & inode,
  bool & treeRootBlockIsDirty,
  bool initializeInode)
  : m_blockStorage(blockStorage)
  , m_storage(blockStorage.storage())
  , m_inode(inode)
  , m_treeRootBlockIsDirty(treeRootBlockIsDirty)
{
  if (initializeInode)
  {
    m_inode.levelsCount = 0;
    m_inode.directReferences.itemsCount = 0;
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
      format::BlockRangesLeafNode leaf;
      util::readT(m_storage, BlockAddress::fromBlockIndex(m_position->block.nextLeafNode).absoluteAddress(), leaf);
      m_position->block = leaf;
      m_position->indexInBlock = 0;
      m_position->range = OffsetAndSize(leaf.ranges[0].blockIndex(), leaf.ranges[0].blocksCount);
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
    seekInNode(blockIndex, m_position->block.ranges, m_position->block.itemsCount);
  }
  else
  {
    if (m_inode.levelsCount > 0)
    {
      seekInNode(m_inode.levelsCount, blockIndex, m_inode.indirectReferences.children, m_inode.indirectReferences.itemsCount);
    }
    else
    {
      seekInNode(blockIndex, m_inode.directReferences.ranges, m_inode.directReferences.itemsCount);
    }
  }
}

void FileBlocks::seekInNode(uint64_t keyBlockIndex, format::BlockRange const * ranges, unsigned itemsCount)
{
  auto position = std::lower_bound(
    ranges,
    ranges + itemsCount,
    keyBlockIndex,
    [](format::BlockRange const & range, uint64_t blockIndex) -> bool {
      return range.fileOffset < blockIndex;
    }
  );
  if (position == ranges + itemsCount
    || position->fileOffset > keyBlockIndex)
    --position;
  if (position->fileOffset > keyBlockIndex || keyBlockIndex >= position->fileOffset + position->blocksCount)
    throw std::runtime_error("Expectation fail: node not found");

  if (!m_position || m_position->block.ranges != ranges)
  {
    m_position = Position();
    std::copy_n(
      ranges,
      itemsCount,
      m_position->block.ranges
    );
    m_position->block.itemsCount = itemsCount;
    m_position->block.nextLeafNode = format::BlockRangesLeafNode::NoNextLeaf;
  }
  m_position->indexInBlock = position - ranges;
  m_position->range = OffsetAndSize(
    position->blockIndex() + (keyBlockIndex - position->fileOffset),
    position->blocksCount - (keyBlockIndex - position->fileOffset));
}

void FileBlocks::seekInNode(unsigned levelsRemain, uint64_t keyBlockIndex, format::ChildNodeReference const * children, unsigned itemsCount)
{
  auto position = std::lower_bound(
    children + 1,
    children + itemsCount,
    keyBlockIndex,
    [](format::ChildNodeReference const & range, uint64_t blockIndex) -> bool 
    {
      return range.fileOffset < blockIndex;
    }
  );
  if (position == children + itemsCount || position->fileOffset != keyBlockIndex)
    --position;
  seekTree(levelsRemain - 1, keyBlockIndex, position->childBlockIndex);
}

void FileBlocks::seekTree(unsigned levelsRemain, uint64_t keyBlockIndex, uint64_t nodeBlock) 
{
  if (levelsRemain == 0)
  {
    format::BlockRangesLeafNode leaf;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), leaf);

    seekInNode(keyBlockIndex, leaf.ranges, leaf.itemsCount);
    m_position->block.nextLeafNode = leaf.nextLeafNode;
  }
  else
  {
    format::BlockRangesInternalNode internal;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), internal);

    seekInNode(levelsRemain, keyBlockIndex, internal.children, internal.itemsCount);
  }
}

namespace
{
  struct RootReferencesTraitsDirect
  {
    static format::FileInode::DirectReferences & getInodeContainer(format::FileInode & inode)
    { return inode.directReferences; }

    typedef format::BlockRangesLeafNode NodeType;
  };

  struct RootReferencesTraitsIndirect
  {
    static format::FileInode::IndirectReferences & getInodeContainer(format::FileInode & inode)
    { return inode.indirectReferences; }

    typedef format::BlockRangesInternalNode NodeType;
  };

  inline format::ChildNodeReference * getItems(format::FileInode::IndirectReferences & container)
  { return container.children; }

  inline format::ChildNodeReference * getItems(format::BlockRangesInternalNode & container)
  { return container.children; }

  inline format::BlockRange * getItems(format::FileInode::DirectReferences & container)
  { return container.ranges; }

  inline format::BlockRange * getItems(format::BlockRangesLeafNode & container)
  { return container.ranges; }
}

void FileBlocks::append(uint64_t numBlocks)
{
  // Make in-memory BlockRangesNode pseudo-record for direct/indirect references "inlined" in inode, then
  // copy values back to inode if inode storage size is enough or create another "standalone" leaf node for them
  // otherwise

  if (m_inode.levelsCount > 0)
  {
    format::BlockRangesInternalNode node;
    appendRootT<RootReferencesTraitsIndirect>(numBlocks, node);
  }
  else
  {
    format::BlockRangesLeafNode leaf;
    leaf.nextLeafNode = leaf.NoNextLeaf;
    appendRootT<RootReferencesTraitsDirect>(numBlocks, leaf);
  }

  m_position.reset();
}

template<class Traits>
void FileBlocks::appendRootT(uint64_t numBlocks, typename Traits::NodeType & node_container)
{
  // Make in-memory BlockRangesNode pseudo-record for direct/indirect references "inlined" in inode, then
  // copy values back to inode if inode storage size is enough or create another "standalone" leaf node for them
  // otherwise
  
  auto & inode_container = Traits::getInodeContainer(m_inode);

  node_container.itemsCount = inode_container.itemsCount;
  std::copy_n(
    getItems(inode_container),
    inode_container.itemsCount,
    getItems(node_container));

  std::vector<format::ChildNodeReference> childrenToAdd =
    appendToTreeNode(m_inode.levelsCount, numBlocks, node_container, m_treeRootBlockIsDirty);
  if (node_container.itemsCount != inode_container.itemsCount)
  {
    if (node_container.itemsCount > inode_container.MaxCount)
    {
      // Move inode references to separate tree node
      BlockAddress newNode = m_blockStorage.allocateBlock();
      util::writeT(m_storage, newNode.absoluteAddress(), node_container);

      format::ChildNodeReference newChildReference;
      newChildReference.childBlockIndex = newNode.index();
      newChildReference.fileOffset = 0;
      childrenToAdd.insert(childrenToAdd.begin(), newChildReference);
    }
    else
    {
      // We shouldn't need another level if inode isn't filled up yet
      assert(childrenToAdd.empty());

      // Append added values back to inode
      std::copy_n(
        getItems(node_container) + inode_container.itemsCount,
        node_container.itemsCount - inode_container.itemsCount,
        getItems(inode_container) + inode_container.itemsCount
      );
      inode_container.itemsCount = node_container.itemsCount;
    }
  }
  else
    assert(childrenToAdd.empty());

  while (childrenToAdd.size() > format::FileInode::IndirectReferences::MaxCount)
  {
    ++m_inode.levelsCount;
    childrenToAdd = createInternalNodes(childrenToAdd.data(), childrenToAdd.data() + childrenToAdd.size());
  }
  if (!childrenToAdd.empty())
  {
    ++m_inode.levelsCount;
    m_inode.indirectReferences.itemsCount = childrenToAdd.size();
    std::copy(
      childrenToAdd.begin(),
      childrenToAdd.end(),
      m_inode.indirectReferences.children
    );
    m_treeRootBlockIsDirty = true;
  }
}

std::vector<format::ChildNodeReference> FileBlocks::appendToTreeNode(
  unsigned,
  uint64_t numBlocks, format::BlockRangesLeafNode & node, bool & isDirty)
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

  isDirty = !newBlockRanges.empty();

  if (node.itemsCount > 0)
  {
    auto & lastBlock = node.ranges[node.itemsCount - 1];
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
  }

  // Fill remaining free places in this block
  for (; newBlocksStart != newBlockRanges.end()
    && node.itemsCount < node.MaxCount;
    ++newBlocksStart, ++node.itemsCount)
  {
    auto & item = node.ranges[node.itemsCount];
    item.setBlockIndex(newBlocksStart->first);
    item.blocksCount = newBlocksStart->second;
    item.fileOffset = positonInFile;
    positonInFile += item.blocksCount;
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
    node.nextLeafNode = newLeafs.front().index();
    for (auto newLeafIndexIt = newLeafs.begin(); newLeafIndexIt != newLeafs.end(); ++newLeafIndexIt)
    {
      auto const & newLeafIndex = *newLeafIndexIt;

      newSiblingReferences.push_back(format::ChildNodeReference());
      newSiblingReferences.back().childBlockIndex = newLeafIndex.index();
      newSiblingReferences.back().fileOffset = positonInFile;

      format::BlockRangesLeafNode newLeaf;

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
      util::writeT(m_storage, newLeafIndex.absoluteAddress(), newLeaf);
    }
  }
  return newSiblingReferences;
}

std::vector<format::ChildNodeReference> FileBlocks::appendToTreeNode(
  unsigned levelsRemain, uint64_t numBlocks, format::BlockRangesInternalNode & node, bool & isDirty)
{
  auto newChildrenReferences = appendToTree(
    levelsRemain - 1, 
    numBlocks, 
    node.children[node.itemsCount - 1].childBlockIndex);
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

    format::BlockRangesInternalNode newInternal;
    for (newInternal.itemsCount = 0;
      newInternal.itemsCount < newInternal.MaxCount
      && newChildrenStart != newChildrenEnd;
      ++newInternal.itemsCount, ++newChildrenStart)
    {
      newInternal.children[newInternal.itemsCount] = *newChildrenStart;
    }
    util::writeT(m_storage, newNodeIndex.absoluteAddress(), newInternal);
  }
  return newSiblingReferences;
}

std::vector<format::ChildNodeReference> FileBlocks::appendToTree(unsigned levelsRemain, uint64_t numBlocks, uint64_t nodeBlock)
{
  std::vector<format::ChildNodeReference> result;
  bool isDirty = false;
  if (levelsRemain == 0)
  {
    format::BlockRangesLeafNode leaf;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), leaf);
    result = appendToTreeNode(levelsRemain, numBlocks, leaf, isDirty);
    if (isDirty)
      util::writeT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), leaf);
  }
  else
  {
    format::BlockRangesInternalNode internal;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), internal);
    result = appendToTreeNode(levelsRemain, numBlocks, internal, isDirty);
    if (isDirty)
      util::writeT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), internal);
  }
  
  return result;
}

void FileBlocks::truncateTreeNode(uint64_t newSizeInBlocks, format::BlockRange * ranges, uint16_t & itemsCount, bool & isDirty)
{
  for(; itemsCount > 0; --itemsCount)
  {
    auto & range = ranges[itemsCount - 1];

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

void FileBlocks::truncateTreeNode(
  unsigned levelsRemain, 
  uint64_t newSizeInBlocks, 
  format::ChildNodeReference const * children, 
  uint16_t & itemsCount, 
  bool & isDirty, 
  OnNewRootFunc const & onNewRoot)
{
  for (; itemsCount > 0; --itemsCount)
  {
    if (!truncateTree(
        levelsRemain - 1,
        newSizeInBlocks, 
        children[itemsCount - 1].childBlockIndex,
        itemsCount == 1 ? onNewRoot : OnNewRootFunc())
      )
      break;
    isDirty = true;
  }
}

// Return true if node was deleted or its reference moved to root
bool FileBlocks::truncateTree(unsigned levelsRemain, uint64_t newSizeInBlocks, uint64_t nodeBlock, OnNewRootFunc const & onNewRoot)
{
  bool isDirty = false;
  if (levelsRemain == 0)
  {
    format::BlockRangesLeafNode leaf;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), leaf);
    truncateTreeNode(newSizeInBlocks, leaf.ranges, leaf.itemsCount, isDirty);
    if (leaf.itemsCount == 0)
    {
      m_blockStorage.releaseBlocks(nodeBlock, 1);
      return true;
    }
    else if (leaf.nextLeafNode != format::BlockRangesLeafNode::NoNextLeaf)
    {
      leaf.nextLeafNode = format::BlockRangesLeafNode::NoNextLeaf;
      isDirty = true;
    }
    bool returnValue = false;
    if (onNewRoot)
    {
      if (onNewRoot(nodeBlock, levelsRemain, &leaf, nullptr))
      {
        m_blockStorage.releaseBlocks(nodeBlock, 1);
        return true;
      }
      else
        // Save node and return true - delete ascendants
        returnValue = true;
    }
    if (isDirty)
      util::writeT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), leaf);
    return returnValue;
  }
  else
  {
    format::BlockRangesInternalNode internal;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), internal);
    truncateTreeNode(levelsRemain, newSizeInBlocks, internal.children, internal.itemsCount, isDirty, onNewRoot);
    if (internal.itemsCount == 0)
    {
      m_blockStorage.releaseBlocks(nodeBlock, 1);
      return true;
    }
    bool returnValue = false;
    if (onNewRoot)
    {
      if (onNewRoot(nodeBlock, levelsRemain, nullptr, &internal))
      {
        m_blockStorage.releaseBlocks(nodeBlock, 1);
        return true;
      }
      else
        // Save node and return true - delete ascendants
        returnValue = true;
    }
    if (isDirty)
      util::writeT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), internal);
    return returnValue;
  }
}

void FileBlocks::truncate(uint64_t newSizeInBlocks)
{
  if (m_inode.levelsCount > 0)
  {
    boost::optional<format::BlockRangesLeafNode> newLeafContent;
    boost::optional<format::BlockRangesInternalNode> newInternalContent;
    boost::optional<uint64_t> newRootIndex;
    unsigned newLevel;
    truncateTreeNode(
      m_inode.levelsCount,
      newSizeInBlocks,
      m_inode.indirectReferences.children,
      m_inode.indirectReferences.itemsCount,
      m_treeRootBlockIsDirty,
      [&](uint64_t blockIndex, 
        unsigned level,
        format::BlockRangesLeafNode const * leaf, 
        format::BlockRangesInternalNode const * internal) -> bool
      {
        if (leaf && leaf->itemsCount <= format::FileInode::DirectReferences::MaxCount)
        {
          newLeafContent = *leaf;
          return true; // release original node
        }
        else if (internal && internal->itemsCount <= format::FileInode::IndirectReferences::MaxCount)
        {
          newInternalContent = *internal;
          newLevel = level;
          return true; // release original node
        }
        else
        {
          newRootIndex = blockIndex;
          newLevel = level;
          return false;
        }
      }
    );
    if (newRootIndex)
    {
      m_inode.levelsCount = newLevel + 1;
      m_inode.indirectReferences.itemsCount = 1;
      m_inode.indirectReferences.children[0].childBlockIndex = *newRootIndex;
      m_inode.indirectReferences.children[0].fileOffset = 0;
      m_treeRootBlockIsDirty = true;
    }
    else if (newLeafContent)
    {
      m_inode.levelsCount = 0;
      m_inode.directReferences.itemsCount = newLeafContent->itemsCount;
      std::copy_n(
        newLeafContent->ranges,
        newLeafContent->itemsCount,
        m_inode.directReferences.ranges
      );
      m_treeRootBlockIsDirty = true;
    }
    else if (newInternalContent)
    {
      m_inode.levelsCount = newLevel;
      m_inode.indirectReferences.itemsCount = newInternalContent->itemsCount;
      std::copy_n(
        newInternalContent->children,
        newInternalContent->itemsCount,
        m_inode.indirectReferences.children
      );
      m_treeRootBlockIsDirty = true;
    }
  }
  else
  {
    truncateTreeNode(
      newSizeInBlocks,
      m_inode.directReferences.ranges,
      m_inode.directReferences.itemsCount,
      m_treeRootBlockIsDirty);
  }

  m_position.reset();
}

void FileBlocks::check() const
{
  CheckState state;
  state.filePosition = 0;

  if (m_inode.levelsCount > 0)
    checkTreeNode(m_inode.levelsCount, m_inode.indirectReferences.children, m_inode.indirectReferences.itemsCount, state);
  else
    checkTreeNode(m_inode.directReferences.ranges, m_inode.directReferences.itemsCount, state);

  F2F_FORMAT_ASSERT(!state.lastNextLeafNodeReference 
    || *state.lastNextLeafNodeReference == format::BlockRangesLeafNode::NoNextLeaf);

  F2F_FORMAT_ASSERT(m_inode.blocksCount == state.filePosition);
  F2F_FORMAT_ASSERT(util::FloorDiv(m_inode.fileSize, format::AddressableBlockSize) == m_inode.blocksCount);
}

void FileBlocks::checkTree(unsigned levelsRemain, uint64_t nodeBlock, CheckState & state) const
{
  F2F_FORMAT_ASSERT(state.referencedBlocks.insert(nodeBlock).second);
  m_blockStorage.checkAllocatedBlock(nodeBlock);

  if (levelsRemain == 0)
  {
    format::BlockRangesLeafNode leaf;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), leaf);

    if (state.lastNextLeafNodeReference)
      F2F_FORMAT_ASSERT(nodeBlock == *state.lastNextLeafNodeReference);
    state.lastNextLeafNodeReference = leaf.nextLeafNode;

    F2F_FORMAT_ASSERT(leaf.itemsCount > 0 && leaf.itemsCount <= leaf.MaxCount);
    checkTreeNode(leaf.ranges, leaf.itemsCount, state);
  }
  else
  {
    format::BlockRangesInternalNode internal;
    util::readT(m_storage, BlockAddress::fromBlockIndex(nodeBlock).absoluteAddress(), internal);

    F2F_FORMAT_ASSERT(internal.itemsCount > 0 && internal.itemsCount <= internal.MaxCount);
    checkTreeNode(levelsRemain, internal.children, internal.itemsCount, state);
  }
}

void FileBlocks::checkTreeNode(format::BlockRange const * ranges, uint16_t itemsCount, CheckState & state) const
{
  for (int i = 0; i < itemsCount; ++i)
  {
    auto const & range = ranges[i];
    F2F_FORMAT_ASSERT(range.fileOffset == state.filePosition);
    state.filePosition += range.blocksCount;
    for (unsigned block = 0; block < range.blocksCount; ++block)
    {
      F2F_FORMAT_ASSERT(state.referencedBlocks.insert(range.blockIndex() + block).second);
      m_blockStorage.checkAllocatedBlock(range.blockIndex() + block);
    }
  }
}

void FileBlocks::checkTreeNode(unsigned levelsRemain, 
  format::ChildNodeReference const * children, uint16_t itemsCount, CheckState & state) const
{
  for (int i = 0; i < itemsCount; ++i)
  {
    auto const & child = children[i];

    F2F_FORMAT_ASSERT(child.fileOffset == state.filePosition);
    checkTree(levelsRemain - 1, child.childBlockIndex, state);
  }
}

}