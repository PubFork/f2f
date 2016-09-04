#pragma once

#include <boost/detail/endian.hpp>
#include <boost/multiprecision/integer.hpp>

namespace f2f { namespace util {

template<class T1, class T2>
inline T1 FloorDiv(T1 x, T2 y)
{
  // Works only for positive integers
  static_assert(std::is_integral<T1>::value, "");
  static_assert(std::is_integral<T2>::value, "");

  return (x + y - 1) / y;
}

}}