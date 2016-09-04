cmake_minimum_required (VERSION 2.8)

project(f2f)

# Search packages for host system instead of packages for target system
# in case of cross compilation these macro should be defined by toolchain file
if(NOT COMMAND find_host_package)
  macro(find_host_package)
    find_package(${ARGN})
  endmacro()
endif()

find_host_package(Boost REQUIRED)

include_directories(
  ${Boost_INCLUDE_DIRS}
  include
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

if (MSVC)
  set_directory_properties(PROPERTIES 
    COMPILE_DEFINITIONS NOMINMAX
  ) 
endif()

set(FORMAT_SOURCES
  src/format/BlockStorage.hpp 
  src/format/Common.hpp
  src/format/Inode.hpp 
)

source_group("format" FILES ${FORMAT_SOURCES})

set(UTIL_SOURCES
  src/util/BitRange.hpp 
  src/util/StorageT.hpp
  src/util/FloorDiv.hpp 
)

source_group("util" FILES ${UTIL_SOURCES})

set(SOURCES
  src/IStorage.hpp 
  src/BlockStorage.hpp 
  src/BlockStorage.cpp 
  src/FileBlocks.hpp 
  src/FileBlocks.cpp 
#  src/FileBlocksWithDirectRanges.hpp 
#  src/FileBlocksWithDirectRanges.cpp 
  src/File.hpp 
  src/File.cpp 
  src/Exception.hpp
)

source_group("sources" FILES ${SOURCES})

add_library(f2f
  ${FORMAT_SOURCES}
  ${UTIL_SOURCES}
  ${SOURCES}
)

set(TEST_SOURCES
  test/BitRange_test.cpp 
  test/BlockStorage_test.cpp 
  test/File_test.cpp 
  test/StorageInMemory.hpp 
  test/StorageInMemory.cpp 
)

source_group("test" FILES ${TEST_SOURCES})

add_executable(f2f_test
  3rdparty/gtest/src/gtest_main.cc 
  3rdparty/gtest/src/gtest-all.cc 

  ${TEST_SOURCES}

  src/BlockStorage.hpp 
  src/BlockStorage.cpp 
  src/FileBlocks.hpp 
  src/FileBlocks.cpp 
  src/File.hpp 
  src/File.cpp 
)

target_include_directories(f2f_test
  PRIVATE 3rdparty/gtest/include
  PRIVATE 3rdparty/gtest
  PRIVATE src
)
