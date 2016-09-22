#include "DirectoryIteratorImpl.hpp"
#include "f2f/FileSystemError.hpp"

namespace f2f
{

DirectoryEntry::DirectoryEntry()
{}

DirectoryEntry::~DirectoryEntry()
{}

FileType DirectoryEntry::type() const
{
  return m_impl->directory.m_iterator.currentFileType();
}

std::string DirectoryEntry::name() const
{
  // TODO: encoding
  return m_impl->directory.m_iterator.currentName();
}

std::string DirectoryEntry::path() const
{
  return m_impl->directory.m_directoryPath + "/" + name();
}

DirectoryIteratorImpl::DirectoryEntry::DirectoryEntry(DirectoryIteratorImpl & directory)
{
  m_impl = new Impl(directory);
}

DirectoryIteratorImpl::DirectoryEntry::~DirectoryEntry()
{
  delete m_impl;
}

DirectoryIterator::DirectoryIterator()
  : m_impl(nullptr)
{}

DirectoryIterator::DirectoryIterator(DirectoryIterator && src)
  : m_impl(src.m_impl)
{
  src.m_impl = nullptr;
}

DirectoryIterator::DirectoryIterator(DirectoryIterator const & src)
  : m_impl(nullptr)
{
  *this = src;
}

DirectoryIterator::~DirectoryIterator()
{
  delete m_impl;
}

DirectoryIterator & DirectoryIterator::operator=(DirectoryIterator && src)
{
  delete m_impl;
  m_impl = src.m_impl;
  src.m_impl = nullptr;
  return *this;
}

DirectoryIterator & DirectoryIterator::operator=(DirectoryIterator const & src)
{
  delete m_impl;
  if (src.m_impl)
    m_impl = new Impl(*src.m_impl);
  else
    m_impl = nullptr;
  return *this;
}

bool DirectoryIterator::operator!=(DirectoryIterator const & rhs) const
{
  // Simple comparison only for end() check
  return (!m_impl || m_impl->ptr->m_iterator.eof()) != (!rhs.m_impl || rhs.m_impl->ptr->m_iterator.eof());
}

DirectoryIterator & DirectoryIterator::operator++()
{
  if (!m_impl || m_impl->ptr->m_iterator.eof())
    throw FileSystemError(ErrorCode::IncorrectIteratorAccess, "Incrementing end directory iterator");
  if (!m_impl->ptr->m_isIteratorValid())
  {
    delete m_impl;
    m_impl = nullptr;
  }
  else
    m_impl->ptr->m_iterator.moveNext();
  return *this;
}

const DirectoryEntry & DirectoryIterator::operator*() const
{
  if (!m_impl || m_impl->ptr->m_iterator.eof())
    throw FileSystemError(ErrorCode::IncorrectIteratorAccess, "Dereferencing end directory iterator");
  return m_impl->ptr->m_entry;
}

const DirectoryEntry * DirectoryIterator::operator->() const
{
  if (!m_impl || m_impl->ptr->m_iterator.eof())
    throw FileSystemError(ErrorCode::IncorrectIteratorAccess, "Dereferencing end directory iterator");
  return &m_impl->ptr->m_entry;
}

DirectoryIteratorImpl::DirectoryIteratorImpl(
  std::shared_ptr<FileSystemImpl> const & owner,
  std::string const & directoryPath,
  BlockAddress const & inodeAddress,
  std::function<bool()> const & isIteratorValid,
  std::function<void()> const & onFinishIteration)
  : m_owner(owner)
  , m_directoryPath(directoryPath)
  , m_directory(owner->m_blockStorage, inodeAddress)
  , m_iterator(m_directory)
  , m_isIteratorValid(isIteratorValid)
  , m_onFinishIteration(onFinishIteration)
  , m_entry(*this)
{}

DirectoryIteratorImpl::~DirectoryIteratorImpl()
{
  m_onFinishIteration();
}

}