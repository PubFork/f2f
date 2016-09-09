#ifndef _F2F_API_ISTORAGE_H
#define _F2F_API_ISTORAGE_H

#include <cstdint>
#include "f2f/Common.hpp"

namespace f2f
{
  
// Made virtual mostly for mocking purposes
class IStorage
{
public:
  virtual OpenMode openMode() const = 0;
  virtual uint64_t sizeLimit() const = 0;
  virtual uint64_t size() const = 0;
  virtual void read(uint64_t position, size_t size, void *) const = 0; // throw if can't read 'size' bytes
  virtual void write(uint64_t position, size_t size, void const *) = 0;
  virtual void resize(uint64_t size) = 0; // fill with zeros on increase

  virtual ~IStorage() {}
};

}

#endif