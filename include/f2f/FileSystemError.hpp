#ifndef _F2F_API_FILE_SYSTEM_ERROR_H
#define _F2F_API_FILE_SYSTEM_ERROR_H

#include "f2f/Defs.hpp"

namespace f2f
{

enum class ErrorCode
{
  OperationRequiresWriteAccess,
  FileNameExceedsLimit,
  FileLocked,
  FileExists,
  PathNotFound,
  CantRemoveRootDirectory,
  IncorrectIteratorAccess,
  OperationRequiresOpenedFile,
  StorageLimitReached,
  InvalidStorageFormat,
  InternalExpectationFail
};

class F2F_API_DECL FileSystemError
{
public:
  FileSystemError(ErrorCode code, const char * msg);
  FileSystemError(FileSystemError const &);
  FileSystemError(FileSystemError &&);
  ~FileSystemError();

  FileSystemError & operator=(FileSystemError const &);
  FileSystemError & operator=(FileSystemError &&);

  ErrorCode code() const;
  const char * message() const;

private:
  class Impl;
  Impl * m_impl;
};

}

#endif