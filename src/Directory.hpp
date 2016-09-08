#pragma once

#include <vector>
#include "BlockStorage.hpp"
#include "format/Directory.hpp"

namespace f2f
{

typedef std::string utf8string_t;

class Directory
{
public:
  Directory(BlockStorage &);
  Directory(BlockStorage &, BlockAddress const & inodeAddress, OpenMode openMode);

  BlockAddress inodeAddress() const { return m_inodeAddress; }

  void clear();
  void addFile(uint64_t inode, utf8string_t const & fileName);
  boost::optional<uint64_t> searchFile(utf8string_t const & fileName) const;

  void moveFirst();
  void moveNext();
  bool eof() const;
  uint64_t currentFileInode() const;
  utf8string_t currentFileName() const;

  // Diagnostics
  void check() const;

private:
  BlockStorage & m_blockStorage;
  IStorage & m_storage;
  OpenMode const m_openMode;
  BlockAddress m_inodeAddress;
  format::DirectoryInode m_inode;

  void checkOpenMode();

  void read(uint64_t blockIndex, format::DirectoryTreeInternalNode & internalNode) const;
  void read(uint64_t blockIndex, format::DirectoryTreeLeaf & leaf) const;

  typedef uint32_t NameHash_t;
  boost::optional<uint64_t> searchInNode(NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount) const;
  boost::optional<uint64_t> searchInNode(NameHash_t nameHash, utf8string_t const & fileName, format::DirectoryTreeLeafItem const & head, unsigned dataSize) const;
  boost::optional<uint64_t> searchInNode(NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, uint64_t blockIndex) const;

  std::vector<format::DirectoryTreeChildNodeReference> insertInNode(uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, uint64_t blockIndex);
  boost::optional<format::DirectoryTreeChildNodeReference> insertInNode(
    uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName, 
    unsigned levelsRemain, format::DirectoryTreeChildNodeReference * children, 
    uint16_t & itemsCount, unsigned maxItemsCount, bool & isDirty);
  std::vector<format::DirectoryTreeChildNodeReference> insertInNode(
    uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName,
    format::DirectoryTreeLeafItem & head, uint16_t & dataSize, unsigned maxSize, bool & isDirty);
  static unsigned getSizeOfLeafRecord(utf8string_t const & fileName);

  void removeNode(format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount, unsigned levelsRemain);
  void removeNode(format::DirectoryTreeLeafItem const & head, unsigned dataSize);

  struct CheckState
  {
    NameHash_t lastHash;
  };
  void checkNode(CheckState &, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount, unsigned levelsRemain) const;
  void checkNode(CheckState &, format::DirectoryTreeLeafItem const & head, unsigned dataSize) const;
};

}