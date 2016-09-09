#ifndef _F2F_API_FILE_DESCRIPTOR_H
#define _F2F_API_FILE_DESCRIPTOR_H

#include "f2f/Defs.hpp"

namespace f2f
{

class F2F_API_DECL FileDescriptor
{
public:
  FileDescriptor();
  FileDescriptor(FileDescriptor &&);
  FileDescriptor(FileDescriptor const &);
  ~FileDescriptor();

  FileDescriptor & operator=(FileDescriptor const &);
  FileDescriptor & operator=(FileDescriptor &&);

  bool isOpen() const;
  void close();

  void seek(uint64_t position);
  uint64_t position() const;
  void read(size_t & inOutSize, void * buffer);
  void write(size_t size, void const * buffer);
  void truncate();

private:
  friend class FileDescriptorFactory;
  class Impl;
  Impl * m_impl;
};

}

#endif