#pragma once

#include <stdexcept>
#include <assert.h>

namespace f2f
{

struct LogicError: public std::logic_error
{
  explicit LogicError(const char* what_arg)
    : logic_error(what_arg)
  {}
};

struct OpenModeError: public LogicError
{
  explicit OpenModeError(const char* what_arg)
    : LogicError(what_arg)
  {}
};

struct InvalidFormatError: public std::runtime_error
{
  explicit InvalidFormatError(const char* what_arg)
    : runtime_error(std::string("Invalid format: ") + what_arg)
  {}
};

[[noreturn]]
inline void ThrowInvalidFormat(const char * description)
{
  //assert(false);
  throw InvalidFormatError(description);
}

#define F2F_THROW_INVALID_FORMAT(description) ThrowInvalidFormat(description)
#define F2F_FORMAT_ASSERT(expression) \
  (void)((!!(expression)) || (ThrowInvalidFormat("Expectation fail"), false))

}