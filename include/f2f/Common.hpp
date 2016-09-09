#ifndef _F2F_API_COMMON_H
#define _F2F_API_COMMON_H

namespace f2f
{

enum class FileType 
{
  None = 0,
  NotFound = -1,
  Regular = 1,
  Directory = 2
};

enum class OpenMode
{
  ReadOnly,
  ReadWrite
};

}

#endif