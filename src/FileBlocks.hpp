#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include "format/Inode.hpp"
#include "BlockStorage.hpp"

namespace f2f
{

class FileBlocks
{
public:
  typedef std::pair<BlockAddress, unsigned int> OffsetAndSize; /* both in blocks */

  FileBlocks(BlockStorage &, 
    format::FileInode & inode,
    bool & m_treeRootBlockIsDirty,
    bool initializeInode = false);

  bool eof() const;
  void seek(uint64_t blockIndex);
  void moveToNextRange();
  OffsetAndSize const & currentRange() const;
  void append(uint64_t numBlocks);
  void truncate(uint64_t newSizeInBlocks);

  // Diagnostics
  void check() const;

private:
  BlockStorage & m_blockStorage;
  IStorage & m_storage;
  bool & m_treeRootBlockIsDirty;
  format::FileInode & m_inode;

  struct Position
  {
    OffsetAndSize range;
    format::BlockRangesLeafNode block;
    unsigned indexInBlock;
  };
  boost::optional<Position> m_position;

  void seekTree(unsigned levelsRemain, uint64_t blockIndex, BlockAddress nodeBlock);
  void seekInNode(uint64_t keyBlockIndex, format::BlockRange const * ranges, unsigned itemsCount);
  void seekInNode(unsigned levelsRemain, uint64_t keyBlockIndex, format::ChildNodeReference const * children, unsigned itemsCount);
  std::vector<format::ChildNodeReference> appendToTree(unsigned levelsRemain, uint64_t numBlocks, BlockAddress nodeBlock);
  std::vector<format::ChildNodeReference> appendToTreeNode(unsigned levelsRemain, uint64_t numBlocks, format::BlockRangesLeafNode &, bool & isDirty);
  std::vector<format::ChildNodeReference> appendToTreeNode(unsigned levelsRemain, uint64_t numBlocks, format::BlockRangesInternalNode &, bool & isDirty);
  template<class Traits> void appendRootT(uint64_t numBlocks, typename Traits::NodeType &);
  std::vector<format::ChildNodeReference> createInternalNodes(format::ChildNodeReference const * newChildrenStart, format::ChildNodeReference const * newChildrenEnd);
  typedef std::function<bool (BlockAddress, unsigned, format::BlockRangesLeafNode const *, format::BlockRangesInternalNode const *)> OnNewRootFunc;
  bool truncateTree(unsigned levelsRemain, uint64_t newSizeInBlocks, BlockAddress nodeBlock, OnNewRootFunc const & onNewRoot);
  void truncateTreeNode(uint64_t newSizeInBlocks, format::BlockRange * ranges, uint16_t & itemsCount, bool & isDirty);
  void truncateTreeNode(unsigned levelsRemain, uint64_t newSizeInBlocks, format::ChildNodeReference const * children, 
    uint16_t & itemsCount, bool & isDirty, 
    OnNewRootFunc const & onNewRoot);

  struct CheckState
  {
    uint64_t filePosition;
    boost::optional<uint64_t> lastNextLeafNodeReference;
    std::set<uint64_t> referencedBlocks;
  };
  void checkTree(unsigned levelsRemain, BlockAddress nodeBlock, CheckState &) const;
  void checkTreeNode(format::BlockRange const * ranges, uint16_t itemsCount, CheckState &) const;
  void checkTreeNode(unsigned levelsRemain, format::ChildNodeReference const * children, uint16_t itemsCount, CheckState &) const;
};

}