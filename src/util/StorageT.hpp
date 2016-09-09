#pragma once

#include "f2f/IStorage.hpp"
#include "BlockStorage.hpp"

namespace f2f { namespace util {

template<class T>
inline void readT(IStorage const & storage, uint64_t position, T & obj)
{
  static_assert(!std::is_pointer<T>::value, ""); // Protection against pointer reading
  storage.read(position, sizeof(T), &obj);
}

template<class T>
inline void writeT(IStorage & storage, uint64_t position, T const & obj)
{
  static_assert(!std::is_pointer<T>::value, ""); // Protection against pointer writing
  storage.write(position, sizeof(T), &obj);
}

template<class T>
inline void readT(IStorage const & storage, BlockAddress blockIndex, T & obj)
{
  static_assert(!std::is_pointer<T>::value, ""); // Protection against pointer reading
  storage.read(blockIndex.absoluteAddress(), sizeof(T), &obj);
}

template<class T>
inline void writeT(IStorage & storage, BlockAddress blockIndex, T const & obj)
{
  static_assert(!std::is_pointer<T>::value, ""); // Protection against pointer writing
  storage.write(blockIndex.absoluteAddress(), sizeof(T), &obj);
}

}}