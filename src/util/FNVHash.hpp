#pragma once

#include <cstdint>
#include <boost/multiprecision/integer.hpp>

namespace f2f { namespace util 
{

inline uint32_t HashFNV1a_32(const char * it, const char * end)
{
  uint32_t hash = UINT32_C(2166136261);
  for (; it != end; ++it)
  {
    hash ^= *it;
    hash *= UINT32_C(16777619);
  }

  return hash;
}

}}
