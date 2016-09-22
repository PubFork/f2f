#pragma once

#include <assert.h>
#include "f2f/FileSystemError.hpp"

namespace f2f
{

[[noreturn]]
inline void ThrowFilesystemError(ErrorCode code, const char * description)
{
  //assert(false);
  throw FileSystemError(code, description);
}

}

#define F2F_STRINGIFY(s) #s

#define F2F_FORMAT_ASSERT(expression) \
  (void)((!!(expression)) || (ThrowFilesystemError(ErrorCode::InvalidStorageFormat, \
    "Invalid storage format at " __FILE__ " (" F2F_STRINGIFY(__LINE__) ")"), false))

#define F2F_ASSERT(expression) \
  (void)((!!(expression)) || (ThrowFilesystemError(ErrorCode::InternalExpectationFail, \
    "Internal expectation fail at " __FILE__ " (" F2F_STRINGIFY(__LINE__) ")"), false))
