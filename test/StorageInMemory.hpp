#pragma once

#include <vector>
#include "IStorage.hpp"

namespace f2f
{

class StorageInMemory: public IStorage
{
public:
  StorageInMemory(OpenMode);

  OpenMode openMode() const override { return m_mode; }
  uint64_t sizeLimit() const override { return std::numeric_limits<size_t>::max(); }
  uint64_t size() const override { return m_data.size(); }
  void read(uint64_t position, size_t size, char *) const override;
  void write(uint64_t position, size_t size, char const *) override;
  void resize(uint64_t size) override;

  std::vector<char> & data() { return m_data; }

private:
  OpenMode m_mode;
  std::vector<char> m_data;
};

}