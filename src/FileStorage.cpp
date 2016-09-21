#include "f2f/FileStorage.hpp"
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace f2f
{

namespace fs = boost::filesystem;

/* 

TODO: error handling

*/

class FileStorage: public IStorage
{
public:
  FileStorage(fs::path const & fileName, OpenMode openMode)
    : m_fileName(fs::absolute(fileName)) // Reload on truncate shouldn't rely on current directory
    , m_openMode(openMode)
  {
    m_stream.exceptions(std::fstream::failbit | std::fstream::badbit);
    open();
    m_stream.seekg(0, std::ios_base::end);
    m_size = m_stream.tellg();
  }

  uint64_t size() const override { return m_size; }

  void read(uint64_t position, size_t size, void * data) const override
  {
    m_stream.seekg(position);
    m_stream.read(reinterpret_cast<char *>(data),size);
  }

  void write(uint64_t position, size_t size, void const * data) override
  {
    m_stream.seekp(position);
    m_stream.write(reinterpret_cast<const char *>(data), size);
  }

  void resize(uint64_t size) override
  {
    if (size > m_size)
    {
      m_stream.seekp(size - 1);
      char zeroChar = 0;
      m_stream.write(&zeroChar, 1);
    }
    else if (size < m_size)
    {
      m_stream.close();
      boost::filesystem::resize_file(m_fileName, size);
      open();
    }
    m_size = size;
  }

private:
  fs::path const m_fileName;
  OpenMode const m_openMode;
  mutable fs::fstream m_stream;
  uint64_t m_size;

  void open()
  {
    std::ios_base::openmode openmode = std::ios_base::binary | std::ios_base::in;
    if (m_openMode == OpenMode::ReadWrite)
    {
      openmode |= std::ios_base::out;
      if (!fs::exists(m_fileName))
        openmode |= std::ios_base::trunc;
    }
    m_stream.open(m_fileName, openmode);
  }
};

std::unique_ptr<IStorage> OpenFileStorage(const char * fileName, OpenMode openMode)
{
  return std::unique_ptr<IStorage>(new FileStorage(fileName, openMode));
}

std::unique_ptr<IStorage> OpenFileStorage(const wchar_t * fileName,OpenMode openMode)
{
  return std::unique_ptr<IStorage>(new FileStorage(fileName,openMode));
}

}
