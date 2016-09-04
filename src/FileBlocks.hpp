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
    uint64_t & m_treeRootBlockIndex,
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
  uint64_t & m_treeRootBlockIndex;
  bool & m_treeRootBlockIsDirty;
  struct Position
  {
    OffsetAndSize range;
    format::BlockRangesLeafNode block;
    unsigned indexInBlock;
  };
  boost::optional<Position> m_position;

  void seekTree(uint64_t blockIndex, uint64_t nodeBlock);
  void seekInLeaf(uint64_t keyBlockIndex, format::BlockRangesLeafNode const & leaf);
  std::vector<format::ChildNodeReference> appendToTree(uint64_t numBlocks, uint64_t nodeBlock);
  std::vector<format::ChildNodeReference> appendToTreeLeaf(uint64_t numBlocks, format::BlockRangesLeafNode * = nullptr, bool * isDirty = nullptr);
  std::vector<format::ChildNodeReference> appendToTreeInternal(uint64_t numBlocks, format::BlockRangesInternalNode & node, bool & isDirty);
  std::vector<format::ChildNodeReference> createInternalNodes(format::ChildNodeReference const * newChildrenStart, format::ChildNodeReference const * newChildrenEnd);
  bool truncateTree(uint64_t newSizeInBlocks, uint64_t nodeBlock, uint64_t * newRoot);
  void truncateTreeLeaf(uint64_t newSizeInBlocks, format::BlockRangesLeafNode & node, bool & isDirty);
  void truncateTreeInternal(uint64_t newSizeInBlocks, format::BlockRangesInternalNode & node, bool & isDirty, uint64_t * newRoot);

  struct CheckState
  {
    unsigned level;
    uint64_t filePosition;
    boost::optional<unsigned> leafLevel;
    boost::optional<uint64_t> lastNextLeafNodeReference;
    std::set<uint64_t> referencedBlocks;
  };
  void checkTree(uint64_t nodeBlock, CheckState &) const;
};

}