#pragma once

#include <fstream>
#include "f2f/IStorage.hpp"
#include "../src/format/StorageHeader.hpp"

class SparseStorage: public f2f::IStorage
{
public:
  SparseStorage();
  ~SparseStorage();

  uint64_t size() const override { return m_size; }
  void read(uint64_t position, size_t size, void *) const override;
  void write(uint64_t position, size_t size, void const *) override;
  void resize(uint64_t size) override;

private:
  mutable std::fstream m_index;
  mutable std::fstream m_data;
  uint64_t m_size;
  char m_storageHeader[sizeof(f2f::format::StorageHeader)];
};
