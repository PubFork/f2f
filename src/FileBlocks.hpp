#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include "format/Inode.hpp"
#include "BlockStorage.hpp"

/*

TODO: dedicated types for different kinds of address - index/position, blocks/bytes

*/

namespace f2f
{

class FileBlocks
{
public:
  typedef std::pair<uint64_t, unsigned int> OffsetAndSize; /* both in blocks */

  FileBlocks(BlockStorage &, 
    format::Inode & inode,
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
  format::Inode & m_inode;

  struct Position
  {
    OffsetAndSize range;
    format::BlockRangesLeafNode block;
    unsigned indexInBlock;
  };
  boost::optional<Position> m_position;

  void seekTree(uint64_t blockIndex, uint64_t nodeBlock);
  void seekInNode(uint64_t keyBlockIndex, format::BlockRange const * ranges, unsigned itemsCount);
  void seekInNode(uint64_t keyBlockIndex, format::ChildNodeReference const * children, unsigned itemsCount);
  std::vector<format::ChildNodeReference> appendToTree(uint64_t numBlocks, uint64_t nodeBlock);
  std::vector<format::ChildNodeReference> appendToTreeNode(uint64_t numBlocks, format::BlockRangesLeafNode &, bool & isDirty);
  std::vector<format::ChildNodeReference> appendToTreeNode(uint64_t numBlocks, format::BlockRangesInternalNode &, bool & isDirty);
  template<class Traits> void appendRootT(uint64_t numBlocks, format::BlockRangesNode &);
  std::vector<format::ChildNodeReference> createInternalNodes(format::ChildNodeReference const * newChildrenStart, format::ChildNodeReference const * newChildrenEnd);
  typedef std::function<bool (uint64_t, format::BlockRangesNode const &)> OnNewRootFunc;
  bool truncateTree(uint64_t newSizeInBlocks, uint64_t nodeBlock, OnNewRootFunc const & onNewRoot);
  void truncateTreeNode(uint64_t newSizeInBlocks, format::BlockRange * ranges, uint16_t & itemsCount, bool & isDirty);
  void truncateTreeNode(uint64_t newSizeInBlocks, format::ChildNodeReference const * children, 
    uint16_t & itemsCount, bool & isDirty, 
    OnNewRootFunc const & onNewRoot);

  struct CheckState
  {
    unsigned level;
    uint64_t filePosition;
    boost::optional<unsigned> leafLevel;
    boost::optional<uint64_t> lastNextLeafNodeReference;
    std::set<uint64_t> referencedBlocks;
  };
  void checkTree(uint64_t nodeBlock, CheckState &) const;
  void checkTreeNode(format::BlockRange const * ranges, uint16_t itemsCount, CheckState &) const;
  void checkTreeNode(format::ChildNodeReference const * children, uint16_t itemsCount, CheckState &) const;
};

}