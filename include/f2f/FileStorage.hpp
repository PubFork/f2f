#ifndef _F2F_API_FILE_STORAGE_H
#define _F2F_API_FILE_STORAGE_H

#include <memory>
#include "f2f/Common.hpp"
#include "f2f/Defs.hpp"
#include "f2f/IStorage.hpp"

namespace f2f
{

F2F_API_DECL std::unique_ptr<IStorage> OpenFileStorage(const char * fileName, OpenMode = OpenMode::ReadWrite);
F2F_API_DECL std::unique_ptr<IStorage> OpenFileStorage(const wchar_t * fileName, OpenMode = OpenMode::ReadWrite);

}

#endif