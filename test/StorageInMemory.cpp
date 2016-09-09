#include "StorageInMemory.hpp"

namespace f2f
{

StorageInMemory::StorageInMemory(OpenMode mode)
  : m_mode(mode)
{
}

void StorageInMemory::read(uint64_t position, size_t size, void * data) const
{
  if (position + size > m_data.size())
    throw std::runtime_error("Unexpected");
  std::copy_n(m_data.data() + position, size, reinterpret_cast<char *>(data));
}

void StorageInMemory::write(uint64_t position, size_t size, void const * data)
{
  if (m_mode != OpenMode::ReadWrite)
    throw std::runtime_error("Incorrect file mode");
  if (position + size > m_data.size())
    throw std::runtime_error("Unexpected");
  std::copy_n(reinterpret_cast<const char *>(data), size, m_data.data() + position);
}

void StorageInMemory::resize(uint64_t size)
{
  if (m_mode != OpenMode::ReadWrite)
    throw std::runtime_error("Incorrect file mode");
  if (size > std::numeric_limits<size_t>::max())
    throw std::runtime_error("Unexpected");
  m_data.resize(static_cast<size_t>(size));
}

}