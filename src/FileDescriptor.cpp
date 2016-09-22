#include "FileDescriptorImpl.hpp"
#include "f2f/FileSystemError.hpp"

namespace f2f
{

FileDescriptor::FileDescriptor()
  : m_impl(nullptr)
{}

FileDescriptor::FileDescriptor(FileDescriptor && src)
  : m_impl(src.m_impl)
{
  src.m_impl = nullptr;
}

FileDescriptor::FileDescriptor(FileDescriptor const & src)
  : m_impl(nullptr)
{
  if (src.m_impl)
    m_impl = new Impl(*src.m_impl);
}

FileDescriptor::~FileDescriptor()
{
  delete m_impl;
}

FileDescriptor & FileDescriptor::operator=(FileDescriptor const & src)
{
  if (m_impl)
  {
    m_impl->destroy();
    delete m_impl;
  }
  if (src.m_impl)
    m_impl = new Impl(*src.m_impl);
  else
    m_impl = nullptr;
  return *this;
}

FileDescriptor & FileDescriptor::operator=(FileDescriptor && src)
{
  if (m_impl)
  {
    m_impl->destroy();
    delete m_impl;
  }
  m_impl = src.m_impl;
  src.m_impl = nullptr;
  return *this;
}

bool FileDescriptor::isOpen() const
{
  return m_impl != nullptr && m_impl->ptr->isOpen();
}

void FileDescriptor::close()
{
  if (m_impl)
  {
    m_impl->ptr->close();
    m_impl->destroy();
    delete m_impl;
    m_impl = nullptr;
  }
}

[[noreturn]]
inline void ThrowNotOpened()
{
  throw FileSystemError(ErrorCode::OperationRequiresOpenedFile, "File isn't opened");
}

void FileDescriptor::seek(uint64_t position)
{
  if (!isOpen())
    ThrowNotOpened();

  m_impl->ptr->file()->seek(position);
}

uint64_t FileDescriptor::position() const
{
  if (!isOpen())
    ThrowNotOpened();

  return m_impl->ptr->file()->position();
}

void FileDescriptor::read(size_t & inOutSize, void * buffer)
{
  if (!isOpen())
    ThrowNotOpened();

  m_impl->ptr->file()->read(inOutSize, buffer);
}

void FileDescriptor::write(size_t size, void const * buffer)
{
  if (!isOpen())
    ThrowNotOpened();

  m_impl->ptr->file()->write(size, buffer);
}

void FileDescriptor::truncate()
{
  if(!isOpen())
    ThrowNotOpened();

  m_impl->ptr->file()->truncate();
}

uint64_t FileDescriptor::size() const
{
  if (!isOpen())
    ThrowNotOpened();

  return m_impl->ptr->file()->size();
}

}