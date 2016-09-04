#pragma once

#include "IStorage.hpp"

namespace f2f { namespace util {

template<class T>
inline void readT(IStorage const & storage, uint64_t position, T & obj)
{
  static_assert(!std::is_pointer<T>::value, ""); // Protection against pointer reading
  storage.read(position, sizeof(T), reinterpret_cast<char *>(&obj));
}

template<class T>
inline void writeT(IStorage & storage, uint64_t position, T const & obj)
{
  static_assert(!std::is_pointer<T>::value, ""); // Protection against pointer writing
  storage.write(position, sizeof(T), reinterpret_cast<char const *>(&obj));
}

}}