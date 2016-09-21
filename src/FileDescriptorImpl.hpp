#pragma once

#include <memory>
#include <functional>
#include "f2f/FileDescriptor.hpp"
#include "File.hpp"

namespace f2f
{

class FileSystemImpl;

class FileDescriptorImpl
{
public:
  typedef std::function<void()> OnCloseFunc_t;

  FileDescriptorImpl(std::unique_ptr<File> && file, std::shared_ptr<FileSystemImpl> const & owner, OnCloseFunc_t const & onClose)
    : m_file(std::move(file))
    , m_owner(owner)
    , m_onClose(onClose)
  {}

  ~FileDescriptorImpl()
  {
    try
    {
      close();
    }
    catch (...)
    {}
  }

  bool isOpen() const
  {
    return bool(m_file);
  }

  void close()
  {
    m_file.reset();
    if (m_onClose)
    {
      m_onClose();
      m_onClose = OnCloseFunc_t();
    }
    m_owner.reset();
  }

  File * file() { return m_file.get(); }

private:
  std::unique_ptr<File> m_file;
  std::shared_ptr<FileSystemImpl> m_owner;

  OnCloseFunc_t m_onClose;
};

class FileDescriptor::Impl
{
public:
  std::shared_ptr<FileDescriptorImpl> ptr;

  // Use this method when deleting object not from destructor
  // to keep exceptions happened on close() from being catched in destructor
  void destroy()
  {
    if (ptr.unique())
      ptr->close();
    ptr.reset();
  }
};

class FileDescriptorFactory
{
public:
  static FileDescriptor create(std::shared_ptr<FileDescriptorImpl> && impl)
  {
    FileDescriptor descriptor;
    descriptor.m_impl = new FileDescriptor::Impl;
    descriptor.m_impl->ptr = std::move(impl);
    return descriptor;
  }
};

}