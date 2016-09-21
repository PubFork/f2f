#pragma once

#include <functional>
#include "f2f/DirectoryIterator.hpp"
#include "Directory.hpp"
#include "FileSystemImpl.hpp"

namespace f2f
{

class DirectoryIteratorImpl
{
public:
  DirectoryIteratorImpl(
    std::shared_ptr<FileSystemImpl> const & owner,
    std::string const & directoryPath,
    BlockAddress const & inodeAddress,
    bool & directoryIsDeleted, 
    std::function<void()> const & onFinishIteration);
  ~DirectoryIteratorImpl();

private:
  std::shared_ptr<FileSystemImpl> const m_owner;
  Directory m_directory;

public:
  std::string const m_directoryPath;
  Directory::Iterator m_iterator;
  bool & m_directoryIsDeleted;
  std::function<void()> m_onFinishIteration;

  class DirectoryEntry: public f2f::DirectoryEntry
  {
  public:
    DirectoryEntry(DirectoryIteratorImpl &);
    ~DirectoryEntry();
  };

  DirectoryEntry m_entry;
};

class DirectoryIterator::Impl
{
public:
  std::shared_ptr<DirectoryIteratorImpl> ptr;
};

class DirectoryIteratorFactory
{
public:
  static DirectoryIterator create(std::unique_ptr<DirectoryIteratorImpl> && impl)
  {
    DirectoryIterator instance;
    instance.m_impl = new DirectoryIterator::Impl;
    instance.m_impl->ptr.reset(impl.release());
    return instance;
  }
};

class DirectoryEntry::Impl
{
public:
  Impl(DirectoryIteratorImpl & directory)
    : directory(directory)
  {}

  DirectoryIteratorImpl & directory;
};

}