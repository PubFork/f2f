#include "f2f/FilesystemError.hpp"
#include <string>

namespace f2f
{

class FileSystemError::Impl
{
public:
  ErrorCode code;
  std::string message;
};

FileSystemError::FileSystemError(ErrorCode code, const char * msg)
  : m_impl(new Impl)
{
  m_impl->message = msg;
  m_impl->code = code;
}

FileSystemError::FileSystemError(FileSystemError && src)
  : m_impl(src.m_impl)
{
  src.m_impl = nullptr;
}

FileSystemError::FileSystemError(FileSystemError const & src)
  : m_impl(new Impl(*src.m_impl))
{
}

FileSystemError::~FileSystemError()
{
  delete m_impl;
}

FileSystemError & FileSystemError::operator=(FileSystemError const & src)
{
  *m_impl = *src.m_impl;
  return *this;
}

FileSystemError & FileSystemError::operator=(FileSystemError && src)
{
  delete m_impl;
  m_impl = src.m_impl;
  src.m_impl = nullptr;
  return *this;
}

ErrorCode FileSystemError::code() const 
{ 
  return m_impl->code; 
}

const char * FileSystemError::message() const 
{ 
  return m_impl->message.c_str(); 
}

}