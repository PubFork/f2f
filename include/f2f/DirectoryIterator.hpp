#ifndef _F2F_API_DIRECTORY_ITERATOR_H
#define _F2F_API_DIRECTORY_ITERATOR_H

#include "f2f/Common.hpp"
#include "f2f/Defs.hpp"

namespace f2f
{

class F2F_API_DECL DirectoryEntry
{
public:
  FileType type() const;
  std::string name() const;
  std::string path() const;

  DirectoryEntry(DirectoryEntry const &) = delete;
  void operator=(DirectoryEntry const &) = delete;

protected:
  DirectoryEntry();
  ~DirectoryEntry();
  class Impl;
  Impl * m_impl;
};

// Directory can be deleted or modified while being iterated.
// In this case next increment of the iterator will make it equal to end()
//
// Similar to C++ InputIterator concept. Only guarantee validity for single pass algorithms: once an 
// iterator has been incremented, all copies of its previous value may be invalidated.
class F2F_API_DECL DirectoryIterator
{
public:
  DirectoryIterator(); // end iterator constructor
  DirectoryIterator(DirectoryIterator &&);
  DirectoryIterator(DirectoryIterator const &);
  ~DirectoryIterator();
  DirectoryIterator & operator=(DirectoryIterator &&);
  DirectoryIterator & operator=(DirectoryIterator const &);

  bool operator!=(DirectoryIterator const &) const;

  DirectoryIterator & operator++();

  const DirectoryEntry & operator*() const;
  const DirectoryEntry * operator->() const;

  class Impl;
private:
  friend class DirectoryIteratorFactory;
  Impl * m_impl;
};

inline DirectoryIterator begin(DirectoryIterator const & it)
{
  return it;
}

inline DirectoryIterator end(DirectoryIterator)
{
  return DirectoryIterator();
}

}

#endif