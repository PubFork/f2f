#pragma once

#include <cstdint>
#include <functional>
#include "BlockStorage.hpp"
#include "FileBlocks.hpp"

namespace f2f
{

class File
{
public:
  File(BlockStorage &); // Create file
  File(BlockStorage &, BlockAddress const & inodeAddress, OpenMode openMode); // Open file

  BlockAddress inodeAddress() const { return m_inodeAddress; }

  void remove();
  void seek(uint64_t position);
  uint64_t position() const { return m_position; }
  void read(size_t & inOutSize, void * buffer);
  void write(size_t size, void const * buffer);
  void truncate();
  uint64_t size() const;

  // Diagnostics
  void check() const;

private:
  BlockStorage & m_blockStorage;
  IStorage & m_storage;
  OpenMode const m_openMode;
  BlockAddress m_inodeAddress;
  format::FileInode m_inode;
  bool m_inodeTreeRootIsDirty;
  FileBlocks m_fileBlocks;
  uint64_t m_position;

  void processData(size_t size, std::function<void(uint64_t, unsigned int)> const & func);
};

}