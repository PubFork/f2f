#pragma once

#include <vector>
#include "f2f/IStorage.hpp"

class StorageInMemory: public f2f::IStorage
{
public:
  StorageInMemory(f2f::OpenMode = f2f::OpenMode::ReadWrite);

  uint64_t size() const override { return m_data.size(); }
  void read(uint64_t position, size_t size, void *) const override;
  void write(uint64_t position, size_t size, void const *) override;
  void resize(uint64_t size) override;

  std::vector<char> & data() { return m_data; }

private:
  f2f::OpenMode m_mode;
  std::vector<char> m_data;
};
