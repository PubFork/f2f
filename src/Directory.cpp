#define _ITERATOR_DEBUG_ARRAY_OVERLOADS 0 // For MSVC

#include "Directory.hpp"
#include "util/Assert.hpp"
#include "util/StorageT.hpp"
#include "util/FNVHash.hpp"
#include "util/Algorithm.hpp"
#include <boost/iterator.hpp>

namespace f2f
{

namespace
{

template<class T>
class DirectoryTreeLeafItemIteratorT
{
public:
  DirectoryTreeLeafItemIteratorT(
    T & head,
    unsigned dataSize)
    : m_head(head)
    , m_dataSize(dataSize)
    , m_position(&head)
  {
    if (!atEnd())
      checkData();
  }

  bool atEnd() const
  {
    return offsetInBytes() >= m_dataSize;
  }

  void operator ++()
  {
    m_position = reinterpret_cast<T*>(m_position->name + m_position->nameSize);
    if (!atEnd())
      checkData();
  }

  T * operator ->() const { return m_position; }
  T & operator * () const { return *m_position; }
  T * get() const { return m_position; }

  template<class OtherIt>
  bool operator ==(OtherIt const & it) const { return m_position == it.m_position; }

  size_t offsetInBytes() const
  {
    return reinterpret_cast<char const *>(m_position) - reinterpret_cast<char const *>(&m_head);
  }

private:
  T & m_head;
  unsigned const m_dataSize;
  T * m_position;

  void checkData() const
  {
    F2F_FORMAT_ASSERT(m_position->nameSize < 0xffff); // Just to not bother with overflow checking
    F2F_FORMAT_ASSERT(reinterpret_cast<char const *>(m_position) >= reinterpret_cast<char const *>(&m_head));
    F2F_FORMAT_ASSERT(m_position->name + m_position->nameSize <= reinterpret_cast<char const *>(&m_head) + m_dataSize);
  }
};

typedef DirectoryTreeLeafItemIteratorT<format::DirectoryTreeLeafItem> DirectoryTreeLeafItemIterator;
typedef DirectoryTreeLeafItemIteratorT<format::DirectoryTreeLeafItem const> DirectoryTreeLeafItemConstIterator;

} // anonymous namespace

const BlockAddress Directory::NoParentDirectory = BlockAddress::fromBlockIndex(std::numeric_limits<uint64_t>::max());

Directory::Directory(BlockStorage & blockStorage, BlockAddress const & parentAddress, create_tag)
  : m_blockStorage(blockStorage)
  , m_storage(blockStorage.storage())
{
  m_inodeAddress = m_blockStorage.allocateBlock();
  memset(&m_inode, 0, sizeof(m_inode));
  m_inode.parentDirectoryInode = 
    parentAddress == NoParentDirectory ? m_inodeAddress.index() : parentAddress.index();
  m_inode.levelsCount = 0;
  m_inode.directReferences.dataSize = 0;
  util::writeT(m_storage, m_inodeAddress, m_inode);
}

Directory::Directory(BlockStorage & blockStorage, BlockAddress const & inodeAddress)
  : m_blockStorage(blockStorage)
  , m_storage(blockStorage.storage())
  , m_inodeAddress(inodeAddress)
{
  util::readT(m_storage, inodeAddress, m_inode);

  F2F_FORMAT_ASSERT(m_inode.levelsCount < 100);
  if (m_inode.levelsCount > 0)
    F2F_FORMAT_ASSERT(m_inode.indirectReferences.itemsCount <= m_inode.indirectReferences.MaxCount);
  else
    F2F_FORMAT_ASSERT(m_inode.directReferences.dataSize <= m_inode.directReferences.MaxDataSize);
}

BlockAddress Directory::parentInodeAddress() const
{
  return BlockAddress::fromBlockIndex(m_inode.parentDirectoryInode);
}

void Directory::read(BlockAddress blockIndex, format::DirectoryTreeInternalNode & internalNode) const
{
  util::readT(m_storage, blockIndex, internalNode);
  F2F_FORMAT_ASSERT(internalNode.itemsCount <= internalNode.MaxCount);
}

void Directory::read(BlockAddress blockIndex, format::DirectoryTreeLeaf & leaf) const
{
  util::readT(m_storage, blockIndex, leaf);
  F2F_FORMAT_ASSERT(leaf.dataSize <= leaf.MaxDataSize);
}

boost::optional<std::pair<BlockAddress, FileType>> Directory::searchFile(utf8string_t const & fileName) const
{
  static const std::string ParentDirectoryName("..");
  if (fileName == ParentDirectoryName)
    return std::make_pair(parentInodeAddress(), FileType::Directory);

  NameHash_t nameHash = util::HashFNV1a_32(fileName.data(), fileName.data() + fileName.size());
  boost::optional<uint64_t> result;
  if (m_inode.levelsCount > 0)
  {
    result = searchInNode(
      nameHash, 
      fileName, 
      m_inode.levelsCount, 
      m_inode.indirectReferences.children, 
      m_inode.indirectReferences.itemsCount);
  }
  else
  {
    result = searchInNode(
      nameHash,
      fileName,
      m_inode.directReferences.head,
      m_inode.directReferences.dataSize);
  }
  if (result)
    return std::make_pair(
      BlockAddress::fromBlockIndex(*result & ~format::DirectoryTreeLeafItem::DirectoryFlag), 
      (*result & format::DirectoryTreeLeafItem::DirectoryFlag) 
        ? FileType::Directory 
        : FileType::Regular);
  else
    return {};
}

boost::optional<uint64_t> Directory::searchInNode(NameHash_t nameHash, 
  utf8string_t const & fileName, unsigned levelsRemain, 
  format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount) const
{
  auto position = std::lower_bound(
    children + 1,
    children + itemsCount,
    nameHash,
    [](format::DirectoryTreeChildNodeReference const & child, NameHash_t nameHash) -> bool 
    {
      return child.nameHash < nameHash;
    }
  );
  // Key value "K" may be both in branch with "K" key and in previous branch too due to 
  // way how duplicates are handled
  --position;
  // In case of hash collision and long file names more than one branch may contain same hash
  for(; position != children + itemsCount && (position == children || nameHash >= position->nameHash); ++position)
    if (boost::optional<uint64_t> result = searchInNode(
        nameHash, fileName, levelsRemain - 1, 
        BlockAddress::fromBlockIndex(position->childBlockIndex)))
      return result;
  return {};
}

boost::optional<uint64_t> Directory::searchInNode(
  NameHash_t nameHash, 
  utf8string_t const & fileName, 
  format::DirectoryTreeLeafItem const & head, 
  unsigned dataSize) const
{
  for(DirectoryTreeLeafItemConstIterator item(head, dataSize); 
    !item.atEnd() && nameHash >= item->nameHash; 
    ++item)
  {
    if (nameHash == item->nameHash 
      && item->nameSize == fileName.size()
      && std::equal(item->name, item->name + item->nameSize, fileName.begin()))
      return item->inode;
  }
  return {};
}

boost::optional<uint64_t> Directory::searchInNode(NameHash_t nameHash, 
  utf8string_t const & fileName, unsigned levelsRemain, BlockAddress blockIndex) const
{
  if (levelsRemain > 0)
  {
    format::DirectoryTreeInternalNode internalNode;
    read(blockIndex, internalNode);
    return searchInNode(nameHash, fileName, levelsRemain, internalNode.children, internalNode.itemsCount);
  }
  else
  {
    format::DirectoryTreeLeaf leaf;
    read(blockIndex, leaf);
    return searchInNode(nameHash, fileName, leaf.head, leaf.dataSize);
  }
}

std::vector<format::DirectoryTreeChildNodeReference> Directory::insertInNode(
  uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName,
  unsigned levelsRemain, BlockAddress blockIndex)
{
  std::vector<format::DirectoryTreeChildNodeReference> newChildren;
  if (levelsRemain == 0)
  {
    format::DirectoryTreeLeaf leaf;
    read(blockIndex, leaf);
    bool isDirty = false;
    newChildren = insertInNode(inode, nameHash, fileName, 
      leaf.head, leaf.nextLeafNode, leaf.dataSize, format::DirectoryTreeLeaf::MaxDataSize, isDirty);
    if (isDirty)
      util::writeT(m_storage, blockIndex, leaf);
  }
  else
  {
    format::DirectoryTreeInternalNode internalNode;
    read(blockIndex, internalNode);
    bool isDirty = false;
    auto newChild = insertInNode(inode, nameHash, fileName, levelsRemain, internalNode.children,
      internalNode.itemsCount, internalNode.MaxCount, isDirty);
    if (isDirty)
      util::writeT(m_storage, blockIndex, internalNode);
    if (newChild)
      newChildren.push_back(*newChild);
  }
  return newChildren;
}

boost::optional<format::DirectoryTreeChildNodeReference> Directory::insertInNode(
  uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName,
  unsigned levelsRemain, format::DirectoryTreeChildNodeReference * children, 
  uint16_t & itemsCount, unsigned maxItemsCount, bool & isDirty)
{
  F2F_ASSERT(levelsRemain > 0);

  auto position = std::lower_bound(
    children + 1,
    children + itemsCount,
    nameHash,
    [](format::DirectoryTreeChildNodeReference const & child, NameHash_t nameHash) -> bool
    {
      return child.nameHash < nameHash;
    }
  );
  if (position == children + itemsCount || position->nameHash != nameHash)
    --position;

  std::vector<format::DirectoryTreeChildNodeReference> newChildren =
    insertInNode(inode, nameHash, fileName, levelsRemain - 1, BlockAddress::fromBlockIndex(position->childBlockIndex));

  if (!newChildren.empty())
  {
    isDirty = true;
    if (itemsCount + newChildren.size() <= maxItemsCount)
    {
      std::copy_backward(
        position + 1,
        children + itemsCount,
        children + itemsCount + newChildren.size());
      std::copy(
        newChildren.begin(),
        newChildren.end(),
        position + 1);
      itemsCount += newChildren.size();
    }
    else
    {
      BlockAddress newBlock = m_blockStorage.allocateBlock();
      format::DirectoryTreeInternalNode newNode;
      newNode.itemsCount = (itemsCount + newChildren.size()) / 2;
      unsigned itemsCountToLeave = itemsCount + newChildren.size() - newNode.itemsCount;
      util::InsertAndCopyBackward(
        children, children + itemsCount,
        position + 1,
        newChildren.begin(), newChildren.end(),
        util::MakeSplitOutputIterator(
          boost::make_reverse_iterator(newNode.children + newNode.itemsCount),
          boost::make_reverse_iterator(newNode.children),
          boost::make_reverse_iterator(children + itemsCountToLeave)));

      util::writeT(m_storage, newBlock, newNode);
      itemsCount = itemsCountToLeave;
      format::DirectoryTreeChildNodeReference newNodeReference;
      newNodeReference.childBlockIndex = newBlock.index();
      newNodeReference.nameHash = newNode.children[0].nameHash;
      return newNodeReference;
    }
  }
  return {};
}

// At most two new leaf nodes may be needed (the "worst" case when large file name appears in the middle)
std::vector<format::DirectoryTreeChildNodeReference> Directory::insertInNode(
  uint64_t inode,
  NameHash_t nameHash, utf8string_t const & fileName,
  format::DirectoryTreeLeafItem & head, 
  uint64_t & nextLeafNode,
  uint16_t & dataSize, unsigned maxSize, bool & isDirty)
{
  DirectoryTreeLeafItemIterator position(head, dataSize);
  for(;!position.atEnd() && nameHash >= position->nameHash; ++position)
  {
    if (nameHash == position->nameHash
      && position->nameSize == fileName.size()
      && std::equal(position->name, position->name + position->nameSize, fileName.begin()))
      throw FileExistsError(position->inode & format::DirectoryTreeLeafItem::DirectoryFlag
        ? FileType::Directory
        : FileType::Regular);
  }

  auto insertSize = getSizeOfLeafRecord(fileName);
  auto sumSize = insertSize + dataSize;
  
  char tempBlock[format::DirectoryTreeLeaf::MaxDataSize * 2];
  char * sumData;
  if (sumSize <= maxSize)
  {
    // Perform insert in place
    sumData = reinterpret_cast<char *>(&head);
  }
  else
  {
    // Perform insert in temporary buffer
    sumData = tempBlock;
    memcpy(sumData, &head, position.offsetInBytes());
  }

  memmove(
    sumData + position.offsetInBytes() + insertSize,
    reinterpret_cast<const char *>(position.get()),
    dataSize - position.offsetInBytes());

  format::DirectoryTreeLeafItem & destPosition = 
    *reinterpret_cast<format::DirectoryTreeLeafItem *>(sumData + position.offsetInBytes());
  destPosition.inode = inode;
  destPosition.nameHash = nameHash;
  destPosition.nameSize = fileName.size();
  std::copy(fileName.begin(), fileName.end(), destPosition.name);

  if (sumSize <= maxSize)
  {
    dataSize = sumSize;
    isDirty = true;
    return {};
  }
  else
  {
    // Have to split on 2 or 3
    unsigned mid = sumSize / 2;
    unsigned beforeMid = 0, afterMid = sumSize;
    format::DirectoryTreeLeafItem const & destHead =
      *reinterpret_cast<format::DirectoryTreeLeafItem *>(sumData);
    for (DirectoryTreeLeafItemConstIterator item(destHead, sumSize); !item.atEnd(); ++item)
    {
      if (item.offsetInBytes() > mid)
      {
        afterMid = item.offsetInBytes();
        break;
      }
      else
        beforeMid = item.offsetInBytes();
    }

    std::vector<format::DirectoryTreeChildNodeReference> newLeafsReferences;
    if (std::min(afterMid, sumSize - beforeMid) <= format::DirectoryTreeLeafHeader::MaxDataSize)
    {
      // We can split on 2
      auto splitBy = (afterMid < sumSize - beforeMid) ? afterMid : beforeMid;
      BlockAddress newBlock = m_blockStorage.allocateBlock();
      format::DirectoryTreeLeaf newLeaf;
      newLeaf.nextLeafNode = nextLeafNode;
      nextLeafNode = newBlock.index();
      newLeaf.dataSize = sumSize - splitBy;
      memcpy(&newLeaf.head, sumData + splitBy, newLeaf.dataSize);
      util::writeT(m_storage, newBlock, newLeaf);
      format::DirectoryTreeChildNodeReference reference;
      reference.childBlockIndex = newBlock.index();
      reference.nameHash = newLeaf.head.nameHash;
      newLeafsReferences.push_back(reference);

      dataSize = splitBy;
      memcpy(&head, sumData, dataSize); 
      isDirty = true;
    }
    else
    {
      // 3 leafs are required
      BlockAddress newBlock1 = m_blockStorage.allocateBlock();
      BlockAddress newBlock2 = m_blockStorage.allocateBlock();

      // 1st new block
      format::DirectoryTreeLeaf newLeaf;
      newLeaf.nextLeafNode = newBlock2.index();
      newLeaf.dataSize = afterMid - beforeMid;
      memcpy(&newLeaf.head, sumData + beforeMid, newLeaf.dataSize);
      util::writeT(m_storage, newBlock1, newLeaf);

      format::DirectoryTreeChildNodeReference reference;
      reference.childBlockIndex = newBlock1.index();
      reference.nameHash = newLeaf.head.nameHash;
      newLeafsReferences.push_back(reference);

      // 2nd new block
      newLeaf.nextLeafNode = nextLeafNode;
      newLeaf.dataSize = sumSize - afterMid;
      memcpy(&newLeaf.head, sumData + afterMid, newLeaf.dataSize);
      util::writeT(m_storage, newBlock2, newLeaf);

      reference.childBlockIndex = newBlock2.index();
      reference.nameHash = newLeaf.head.nameHash;
      newLeafsReferences.push_back(reference);

      nextLeafNode = newBlock1.index();
      dataSize = beforeMid;
      memcpy(&head, sumData, dataSize);
      isDirty = true;
    }
    return newLeafsReferences;
  }
}

unsigned Directory::getSizeOfLeafRecord(utf8string_t const & fileName)
{
  return offsetof(format::DirectoryTreeLeafItem, name) + fileName.size();
}

void Directory::addFile(BlockAddress inodeAddress, FileType fileType, utf8string_t const & fileName)
{
  uint64_t inode;
  switch (fileType)
  {
  case f2f::FileType::Regular:
    inode = inodeAddress.index();
    break;
  case f2f::FileType::Directory:
    inode = inodeAddress.index() | format::DirectoryTreeLeafItem::DirectoryFlag;
    break;
  default:
    F2F_ASSERT(false);
    return;
  }

  bool inodeIsDirty = false;

  NameHash_t nameHash = util::HashFNV1a_32(fileName.data(), fileName.data() + fileName.size());
  if (m_inode.levelsCount == 0)
  {
    auto newRecordSize = getSizeOfLeafRecord(fileName);
    if (m_inode.directReferences.dataSize + newRecordSize > m_inode.directReferences.MaxDataSize)
    {
      // Not enough space in root. Need to move root contents and new item into child nodes
      BlockAddress newBlock = m_blockStorage.allocateBlock();
      format::DirectoryTreeLeaf newLeaf;
      newLeaf.nextLeafNode = format::DirectoryTreeLeaf::NoNextLeaf;
      newLeaf.dataSize = m_inode.directReferences.dataSize;
      memcpy(&newLeaf.head, &m_inode.directReferences.head, m_inode.directReferences.dataSize);

      bool isNewLeafDirty;
      std::vector<format::DirectoryTreeChildNodeReference> newChildrenReferences = insertInNode(
        inode, nameHash, fileName, 
        newLeaf.head, newLeaf.nextLeafNode, newLeaf.dataSize, newLeaf.MaxDataSize, isNewLeafDirty);

      util::writeT(m_storage, newBlock, newLeaf);
      m_inode.levelsCount = 1;
      m_inode.indirectReferences.itemsCount = 1;
      m_inode.indirectReferences.children[0].nameHash = 0;
      m_inode.indirectReferences.children[0].childBlockIndex = newBlock.index();

      for(auto const & newChild: newChildrenReferences)
      {
        // Second child node was needed
        m_inode.indirectReferences.children[m_inode.indirectReferences.itemsCount] = newChild;
        ++m_inode.indirectReferences.itemsCount;
      }
      inodeIsDirty = true;
    }
    else
    {
      uint64_t nextLeafNode = 0; // shouldn't be used
      std::vector<format::DirectoryTreeChildNodeReference> newChildrenReferences = insertInNode(
        inode, nameHash, fileName, 
        m_inode.directReferences.head, 
        nextLeafNode,
        m_inode.directReferences.dataSize,
        m_inode.directReferences.MaxDataSize,
        inodeIsDirty);
      F2F_ASSERT(newChildrenReferences.empty());
    }
  }
  else
  {
    if (m_inode.indirectReferences.itemsCount + 2 > m_inode.indirectReferences.MaxCount)
    {
      // At most two items may be added. Splitting root in advance to simplify the code.
      BlockAddress newBlock1 = m_blockStorage.allocateBlock();
      BlockAddress newBlock2 = m_blockStorage.allocateBlock();
      format::DirectoryTreeInternalNode newNode;

      unsigned nodesInBlock1 = m_inode.indirectReferences.itemsCount / 2;
      newNode.itemsCount = nodesInBlock1;
      std::copy_n(
        m_inode.indirectReferences.children,
        newNode.itemsCount,
        newNode.children);
      util::writeT(m_storage, newBlock1, newNode);

      newNode.itemsCount = m_inode.indirectReferences.itemsCount - nodesInBlock1;
      std::copy_n(
        m_inode.indirectReferences.children + nodesInBlock1,
        newNode.itemsCount,
        newNode.children);
      util::writeT(m_storage, newBlock2, newNode);

      ++m_inode.levelsCount;
      m_inode.indirectReferences.itemsCount = 2;
      m_inode.indirectReferences.children[0].childBlockIndex = newBlock1.index();
      m_inode.indirectReferences.children[0].nameHash = 0;
      m_inode.indirectReferences.children[1].childBlockIndex = newBlock2.index();
      m_inode.indirectReferences.children[1].nameHash = newNode.children[0].nameHash;
      inodeIsDirty = true;
    }
    boost::optional<format::DirectoryTreeChildNodeReference> newChild = insertInNode(
      inode, nameHash, fileName, m_inode.levelsCount, 
      m_inode.indirectReferences.children,
      m_inode.indirectReferences.itemsCount, 
      m_inode.indirectReferences.MaxCount,
      inodeIsDirty);
    F2F_ASSERT(!newChild);
  }
  if (inodeIsDirty)
    util::writeT(m_storage, m_inodeAddress, m_inode);
}

boost::optional<uint64_t> Directory::removeFromNode(NameHash_t nameHash, utf8string_t const & fileName,
  unsigned levelsRemain, BlockAddress blockIndex)
{
  boost::optional<uint64_t> removedInode;
  if (levelsRemain == 0)
  {
    format::DirectoryTreeLeaf leaf;
    read(blockIndex, leaf);
    bool isDirty = false;
    removedInode = removeFromNode(nameHash, fileName,
      leaf.head, leaf.dataSize, isDirty);
    if (isDirty)
      util::writeT(m_storage, blockIndex, leaf);
  }
  else
  {
    format::DirectoryTreeInternalNode internalNode;
    read(blockIndex, internalNode);
    bool isDirty = false;
    removedInode = removeFromNode(nameHash, fileName, levelsRemain, internalNode.children,
      internalNode.itemsCount, isDirty);
    if (isDirty)
      util::writeT(m_storage, blockIndex, internalNode);
  }
  return removedInode;
}

boost::optional<uint64_t> Directory::removeFromNode(
  NameHash_t nameHash, utf8string_t const & fileName,
  unsigned levelsRemain, format::DirectoryTreeChildNodeReference * children,
  uint16_t & itemsCount, bool & isDirty)
{
  auto position = std::lower_bound(
    children + 1,
    children + itemsCount,
    nameHash,
    [](format::DirectoryTreeChildNodeReference const & child, NameHash_t nameHash) -> bool 
    {
      return child.nameHash < nameHash;
    }
  );
  // Key value "K" may be both in branch with "K" key and in previous branch too due to 
  // way how duplicates are handled
  --position;
  // In case of hash collision and long file names more than one branch may contain same hash
  for(; position != children + itemsCount && (position == children || nameHash >= position->nameHash); ++position)
    if (boost::optional<uint64_t> result = removeFromNode(
        nameHash, fileName, levelsRemain - 1, 
        BlockAddress::fromBlockIndex(position->childBlockIndex)))
      return result;
  return {};
}

boost::optional<uint64_t> Directory::removeFromNode(
  NameHash_t nameHash, utf8string_t const & fileName,
  format::DirectoryTreeLeafItem & head, uint16_t & dataSize, bool & isDirty)
{
  for (DirectoryTreeLeafItemConstIterator item(head, dataSize);
    !item.atEnd() && nameHash >= item->nameHash;
    ++item)
  {
    if (nameHash == item->nameHash
      && item->nameSize == fileName.size()
      && std::equal(item->name, item->name + item->nameSize, fileName.begin()))
    {
      isDirty = true;

      auto inode = item->inode;

      auto offset = item.offsetInBytes();
      ++item;
      if (item.atEnd())
      {
        // Removing last item - just change dataSize field
        dataSize = offset; 
      }
      else
      {
        auto removedSize = item.offsetInBytes() - offset;
        memmove(
          reinterpret_cast<char *>(&head) + offset, 
          reinterpret_cast<char *>(&head) + item.offsetInBytes(),
          dataSize - item.offsetInBytes()
          );
        dataSize -= removedSize;
      }

      return inode;
    }
  }
  return {};
}

boost::optional<std::pair<BlockAddress, FileType>> Directory::removeFile(utf8string_t const & fileName) 
{
  bool inodeIsDirty = false;
  boost::optional<uint64_t> removedInode;

  NameHash_t nameHash = util::HashFNV1a_32(fileName.data(), fileName.data() + fileName.size());
  if (m_inode.levelsCount == 0)
  {
    removedInode = removeFromNode(
      nameHash, fileName,
      m_inode.directReferences.head,
      m_inode.directReferences.dataSize,
      inodeIsDirty);
  }
  else
  {
    removedInode = removeFromNode(
      nameHash, fileName, m_inode.levelsCount,
      m_inode.indirectReferences.children,
      m_inode.indirectReferences.itemsCount,
      inodeIsDirty);
  }
  if (inodeIsDirty)
    util::writeT(m_storage, m_inodeAddress, m_inode);

  if (removedInode)
    return std::make_pair(
      BlockAddress::fromBlockIndex(*removedInode & ~format::DirectoryTreeLeafItem::DirectoryFlag),
      (*removedInode & format::DirectoryTreeLeafItem::DirectoryFlag)
        ? FileType::Directory
        : FileType::Regular);
  else
    return {};
}

void Directory::remove(OnDeleteFileFunc_t const & onDeleteFile)
{
  if (m_inode.levelsCount == 0)
    removeNode(onDeleteFile, m_inode.directReferences.head, m_inode.directReferences.dataSize);
  else
    removeNode(onDeleteFile, m_inode.indirectReferences.children, m_inode.indirectReferences.itemsCount, m_inode.levelsCount);

  m_blockStorage.releaseBlocks(m_inodeAddress, 1);
}

void Directory::removeNode(OnDeleteFileFunc_t const & onDeleteFile, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount, unsigned levelsRemain)
{
  for(unsigned i = 0; i < itemsCount; ++i)
  {
    if (levelsRemain == 1)
    {
      format::DirectoryTreeLeaf leaf;
      read(BlockAddress::fromBlockIndex(children[i].childBlockIndex), leaf);
      removeNode(onDeleteFile, leaf.head, leaf.dataSize);
    }
    else
    {
      format::DirectoryTreeInternalNode internalNode;
      read(BlockAddress::fromBlockIndex(children[i].childBlockIndex), internalNode);
      removeNode(onDeleteFile, internalNode.children, internalNode.itemsCount, levelsRemain - 1);
    }
    m_blockStorage.releaseBlocks(BlockAddress::fromBlockIndex(children[i].childBlockIndex), 1);
  }
}

void Directory::removeNode(OnDeleteFileFunc_t const & onDeleteFile, format::DirectoryTreeLeafItem const & head, unsigned dataSize)
{
  for (DirectoryTreeLeafItemConstIterator item(head, dataSize); !item.atEnd(); ++item)
  {
    onDeleteFile(
      BlockAddress::fromBlockIndex(item->inode & ~format::DirectoryTreeLeafItem::DirectoryFlag), 
      (item->inode & format::DirectoryTreeLeafItem::DirectoryFlag)
        ? FileType::Directory
        : FileType::Regular);
  }
}

struct Directory::CheckState
{
  NameHash_t lastHash;
  boost::optional<uint64_t> nextLeadNode;
};

void Directory::check() const
{
  CheckState state;
  state.lastHash = 0;

  if (m_inode.levelsCount == 0)
    checkNode(state, m_inode.directReferences.head, m_inode.directReferences.dataSize);
  else
    checkNode(state, m_inode.indirectReferences.children, m_inode.indirectReferences.itemsCount, m_inode.levelsCount);

  F2F_FORMAT_ASSERT(!state.nextLeadNode 
    || *state.nextLeadNode == format::DirectoryTreeLeaf::NoNextLeaf);
}

void Directory::checkNode(CheckState & checkState, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount, unsigned levelsRemain) const
{
  F2F_FORMAT_ASSERT(itemsCount <= format::DirectoryTreeInternalNode::MaxCount);
  for(unsigned i=0; i<itemsCount; ++i)
  {
    if (i > 0)
    {
      F2F_FORMAT_ASSERT(children[i].nameHash >= checkState.lastHash);
      checkState.lastHash = children[i].nameHash;
    }
    m_blockStorage.checkAllocatedBlock(BlockAddress::fromBlockIndex(children[i].childBlockIndex));
    if (levelsRemain == 1)
    {
      format::DirectoryTreeLeaf leaf;
      read(BlockAddress::fromBlockIndex(children[i].childBlockIndex), leaf);
      checkNode(checkState, leaf.head, leaf.dataSize);
      if (checkState.nextLeadNode)
        F2F_FORMAT_ASSERT(*checkState.nextLeadNode == children[i].childBlockIndex);
      checkState.nextLeadNode = leaf.nextLeafNode;
    }
    else
    {
      format::DirectoryTreeInternalNode internalNode;
      read(BlockAddress::fromBlockIndex(children[i].childBlockIndex), internalNode);
      checkNode(checkState, internalNode.children, internalNode.itemsCount, levelsRemain - 1);
    }
  }
}

void Directory::checkNode(CheckState & checkState, format::DirectoryTreeLeafItem const & head, unsigned dataSize) const
{
  F2F_FORMAT_ASSERT(dataSize <= format::DirectoryTreeLeaf::MaxDataSize);
  for (DirectoryTreeLeafItemConstIterator item(head, dataSize); !item.atEnd(); ++item)
  {
    F2F_FORMAT_ASSERT(item->nameSize <= format::MaxFileNameSize);
    NameHash_t nameHash = util::HashFNV1a_32(item->name, item->name + item->nameSize);
    F2F_FORMAT_ASSERT(nameHash == item->nameHash);
    F2F_FORMAT_ASSERT(item->nameHash >= checkState.lastHash);
    checkState.lastHash = item->nameHash;
  }
}

struct Directory::Iterator::Iterator::Impl
{
  Impl(Directory const & directory)
    : directory(directory)
  {}

  Directory const & directory;
  format::DirectoryTreeLeaf currentLeaf;
  std::unique_ptr<DirectoryTreeLeafItemConstIterator> iterator;
};

Directory::Iterator::Iterator(Directory const & directory)
  : m_impl(new Impl(directory))
{
  if (directory.m_inode.levelsCount == 0)
  {
    m_impl->currentLeaf.nextLeafNode = format::DirectoryTreeLeaf::NoNextLeaf;
    m_impl->currentLeaf.dataSize = directory.m_inode.directReferences.dataSize;
    memcpy(&m_impl->currentLeaf.head, &directory.m_inode.directReferences.head, 
      directory.m_inode.directReferences.dataSize);
  }
  else
  {
    BlockAddress blockIndex = BlockAddress::fromBlockIndex(
      directory.m_inode.indirectReferences.children[0].childBlockIndex);
    for(int level = 1; level < directory.m_inode.levelsCount; ++level)
    {
      format::DirectoryTreeInternalNode internalNode;
      directory.read(blockIndex, internalNode);
      blockIndex = BlockAddress::fromBlockIndex(internalNode.children[0].childBlockIndex);
    }
    directory.read(blockIndex, m_impl->currentLeaf);
  }
  m_impl->iterator.reset(new DirectoryTreeLeafItemConstIterator(m_impl->currentLeaf.head, m_impl->currentLeaf.dataSize));
}

Directory::Iterator::~Iterator()
{}

void Directory::Iterator::moveNext()
{
  ++*m_impl->iterator;
  if (m_impl->iterator->atEnd())
    if (m_impl->currentLeaf.nextLeafNode != format::DirectoryTreeLeaf::NoNextLeaf)
    {
      m_impl->directory.read(BlockAddress::fromBlockIndex(m_impl->currentLeaf.nextLeafNode), m_impl->currentLeaf);
      m_impl->iterator.reset(new DirectoryTreeLeafItemConstIterator(m_impl->currentLeaf.head, m_impl->currentLeaf.dataSize));
    }
}

bool Directory::Iterator::eof() const
{
  return m_impl->iterator->atEnd() && m_impl->currentLeaf.nextLeafNode == format::DirectoryTreeLeaf::NoNextLeaf;
}

BlockAddress Directory::Iterator::currentInode() const
{
  return BlockAddress::fromBlockIndex(
    (*m_impl->iterator)->inode & ~format::DirectoryTreeLeafItem::DirectoryFlag);
}

FileType Directory::Iterator::currentFileType() const
{
  return ((*m_impl->iterator)->inode & format::DirectoryTreeLeafItem::DirectoryFlag)
    ? FileType::Directory
    : FileType::Regular;
}

utf8string_t Directory::Iterator::currentName() const
{
  return utf8string_t((*m_impl->iterator)->name, (*m_impl->iterator)->name + (*m_impl->iterator)->nameSize);
}

Directory::FileExistsError::FileExistsError(FileType fileType)
  : runtime_error("File or directory with same name already exists in directory")
  , m_fileType(fileType)
{}

}
