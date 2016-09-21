#include "SparseStorage.hpp"
#include <boost/filesystem.hpp>
#include "../src/format/Common.hpp"

namespace
{
  static const char IndexFileName[] = "f2f_test_index.bin";
  static const char DataFileName[] = "f2f_test_data.bin";
}

SparseStorage::SparseStorage()
  : m_size(0)
{
  m_index.open(IndexFileName, std::ios_base::binary);
  m_data.open(DataFileName, std::ios_base::binary);
  char c = 1;
  m_data.write(&c, 1); // To reserve address 0 in file
}

SparseStorage::~SparseStorage()
{
  m_index.close();
  m_data.close();
  /*boost::filesystem::remove(IndexFileName);
  boost::filesystem::remove(DataFileName);*/
}

void SparseStorage::read(uint64_t position, size_t size, void * data) const 
{
  char * cData = reinterpret_cast<char *>(data);
  if (position < sizeof(f2f::format::StorageHeader))
  {
    auto chunkSize = std::max(uint64_t(size), sizeof(f2f::format::StorageHeader) - position);
    memcpy(cData, m_storageHeader + position, chunkSize);
    cData += chunkSize;
    size -= chunkSize;
    position += chunkSize;
  }
  if (size > 0)
  {
    position -= sizeof(f2f::format::StorageHeader);
    
    m_index.seekg(position / f2f::format::AddressableBlockSize * 8);
    while (size > 0)
    {
      uint64_t indexItem;
      m_index.read(reinterpret_cast<char *>(&indexItem), 8);
      auto chunkSize = std::max(uint64_t(size), f2f::format::AddressableBlockSize - (position % f2f::format::AddressableBlockSize));
      if (indexItem == 0)
        memset(cData, 0, chunkSize);
      else
      {
        char block[f2f::format::AddressableBlockSize];
        m_data.seekg(indexItem);
        m_data.read(block, f2f::format::AddressableBlockSize);
        memcpy(cData, block + position % f2f::format::AddressableBlockSize, chunkSize);
      }
      cData += chunkSize;
      size -= chunkSize;
      position += chunkSize;
    }
  }
}

void SparseStorage::write(uint64_t position, size_t size, void const * data) 
{
  char const * cData = reinterpret_cast<char const *>(data);
  if (position < sizeof(f2f::format::StorageHeader))
  {
    auto chunkSize = std::min(uint64_t(size), sizeof(f2f::format::StorageHeader) - position);
    memcpy(m_storageHeader + position, cData, chunkSize);
    cData += chunkSize;
    size -= chunkSize;
    position += chunkSize;
  }
  if (size > 0)
  {
    position -= sizeof(f2f::format::StorageHeader);
    
    m_index.seekg(position / f2f::format::AddressableBlockSize * 8);
    while (size > 0)
    {
      uint64_t indexItem;
      m_index.read(reinterpret_cast<char *>(&indexItem), 8);
      auto chunkSize = std::min(uint64_t(size), f2f::format::AddressableBlockSize - (position % f2f::format::AddressableBlockSize));
      bool isZeroChunk = std::find_if(cData, cData + chunkSize, [](char c) -> bool {return c != 0;}) == cData + chunkSize;
      if (indexItem == 0 && isZeroChunk)
      {
      }
      else
      {
        char block[f2f::format::AddressableBlockSize] = {};
        
        if (indexItem == 0)
        {
          m_data.seekg(0, std::ios_base::end);
          indexItem = m_data.tellg();
          m_index.seekp(-8, std::ios_base::cur);
          m_index.write(reinterpret_cast<char *>(&indexItem), 8);
        }
        else
        {
          m_data.seekg(indexItem);
          m_data.read(block, f2f::format::AddressableBlockSize);
        }

        memcpy(block + position % f2f::format::AddressableBlockSize, cData, chunkSize);

        m_data.seekp(indexItem);
        m_data.write(block, f2f::format::AddressableBlockSize);
      }
      cData += chunkSize;
      size -= chunkSize;
      position += chunkSize;
    }
  }
}

void SparseStorage::resize(uint64_t size) 
{
  m_index.seekg(0, std::ios_base::end);
  uint64_t fileSize = m_index.tellg();
  uint64_t indexCount = (size - sizeof(f2f::format::StorageHeader) + f2f::format::AddressableBlockSize - 1) / f2f::format::AddressableBlockSize;
  if (indexCount * 8 > fileSize)
  {
    m_index.seekp(indexCount * 8 - 1);
    char c = 0;
    m_index.write(&c, 1);
  }
  m_size = size;
}
