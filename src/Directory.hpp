#pragma once

#include <vector>
#include <memory>
#include "BlockStorage.hpp"
#include "format/Directory.hpp"

namespace f2f
{

typedef std::string utf8string_t;

class Directory
{
public:
  static const BlockAddress NoParentDirectory;

  Directory(BlockStorage &, BlockAddress const & parentAddress); // Create directory
  Directory(BlockStorage &, BlockAddress const & inodeAddress, OpenMode openMode); // Open directory

  BlockAddress inodeAddress() const { return m_inodeAddress; }
  BlockAddress parentInodeAddress() const;

  typedef std::function<void (BlockAddress, FileType)> OnDeleteFileFunc_t;
  void remove(OnDeleteFileFunc_t const &); // Delete this entire directory
  void addFile(BlockAddress inode, FileType, utf8string_t const & fileName);
  boost::optional<std::pair<BlockAddress, FileType>> searchFile(utf8string_t const & fileName) const;
  boost::optional<std::pair<BlockAddress, FileType>> removeFile(utf8string_t const & fileName);

  // Iterator doesn't return '..' record
  class Iterator
  {
  public:
    Iterator(Directory const &);
    ~Iterator();

    void moveNext();
    bool eof() const;

    BlockAddress currentInode() const;
    FileType currentFileType() const;
    utf8string_t currentName() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
  };

  // Diagnostics
  void check() const;

private:
  BlockStorage & m_blockStorage;
  IStorage & m_storage;
  OpenMode const m_openMode;
  BlockAddress m_inodeAddress;
  format::DirectoryInode m_inode;

  void checkOpenMode();

  void read(BlockAddress blockIndex, format::DirectoryTreeInternalNode & internalNode) const;
  void read(BlockAddress blockIndex, format::DirectoryTreeLeaf & leaf) const;

  typedef uint32_t NameHash_t;
  boost::optional<uint64_t> searchInNode(NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount) const;
  boost::optional<uint64_t> searchInNode(NameHash_t nameHash, utf8string_t const & fileName, format::DirectoryTreeLeafItem const & head, unsigned dataSize) const;
  boost::optional<uint64_t> searchInNode(NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, BlockAddress blockIndex) const;

  std::vector<format::DirectoryTreeChildNodeReference> insertInNode(uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, BlockAddress blockIndex);
  boost::optional<format::DirectoryTreeChildNodeReference> insertInNode(
    uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName, 
    unsigned levelsRemain, format::DirectoryTreeChildNodeReference * children, 
    uint16_t & itemsCount, unsigned maxItemsCount, bool & isDirty);
  std::vector<format::DirectoryTreeChildNodeReference> insertInNode(
    uint64_t inode, NameHash_t nameHash, utf8string_t const & fileName,
    format::DirectoryTreeLeafItem & head, uint64_t & nextLeafNode, 
    uint16_t & dataSize, unsigned maxSize, bool & isDirty);
  static unsigned getSizeOfLeafRecord(utf8string_t const & fileName);

  boost::optional<uint64_t> removeFromNode(NameHash_t nameHash, utf8string_t const & fileName, unsigned levelsRemain, BlockAddress blockIndex);
  boost::optional<uint64_t> removeFromNode(
    NameHash_t nameHash, utf8string_t const & fileName,
    unsigned levelsRemain, format::DirectoryTreeChildNodeReference * children,
    uint16_t & itemsCount, bool & isDirty);
  boost::optional<uint64_t> removeFromNode(
    NameHash_t nameHash, utf8string_t const & fileName,
    format::DirectoryTreeLeafItem & head, uint16_t & dataSize, bool & isDirty);

  void removeNode(OnDeleteFileFunc_t const &, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount, unsigned levelsRemain);
  void removeNode(OnDeleteFileFunc_t const &, format::DirectoryTreeLeafItem const & head, unsigned dataSize);

  struct CheckState;
  void checkNode(CheckState &, format::DirectoryTreeChildNodeReference const * children, unsigned itemsCount, unsigned levelsRemain) const;
  void checkNode(CheckState &, format::DirectoryTreeLeafItem const & head, unsigned dataSize) const;
};

}