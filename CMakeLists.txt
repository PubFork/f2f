cmake_minimum_required (VERSION 2.8)

project(f2f)

set(Boost_USE_STATIC_LIBS        ON) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

find_package(Boost REQUIRED COMPONENTS filesystem)

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
  src/format/Directory.hpp
  src/format/File.hpp
  src/format/Inode.hpp 
  src/format/StorageHeader.hpp 
)

source_group("format" FILES ${FORMAT_SOURCES})

set(UTIL_SOURCES
  src/util/Algorithm.hpp 
  src/util/BitRange.hpp 
  src/util/StorageT.hpp
  src/util/FloorDiv.hpp 
  src/util/FNVHash.hpp 
)

source_group("util" FILES ${UTIL_SOURCES})

set(SOURCES
  src/BlockStorage.hpp 
  src/BlockStorage.cpp 
  src/FileBlocks.hpp 
  src/FileBlocks.cpp 
  src/File.hpp 
  src/File.cpp 
  src/FileDescriptorImpl.hpp 
  src/FileDescriptor.cpp 
  src/FileStorage.cpp 
  src/FileSystemImpl.hpp 
  src/FileSystem.cpp 
  src/Directory.hpp 
  src/Directory.cpp 
  src/DirectoryIteratorImpl.hpp 
  src/DirectoryIterator.cpp 
  src/Exception.hpp
)

source_group("sources" FILES ${SOURCES})

set(API_SOURCES
  include/f2f/Common.hpp
  include/f2f/Defs.hpp
  include/f2f/DirectoryIterator.hpp 
  include/f2f/FileSystem.hpp 
  include/f2f/FileDescriptor.hpp 
  include/f2f/IStorage.hpp 
  include/f2f/FileStorage.hpp 
)

source_group("API" FILES ${API_SOURCES})

add_library(f2f SHARED
  ${API_SOURCES}
  ${FORMAT_SOURCES}
  ${UTIL_SOURCES}
  ${SOURCES}
)

target_compile_definitions(f2f
  PRIVATE F2F_API_EXPORTS
)

set_property(TARGET f2f PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(f2f ${Boost_LIBRARIES})

set(TEST_SOURCES
  test/Algorithm_test.cpp 
  test/BitRange_test.cpp 
  test/BlockStorage_test.cpp 
  test/Directory_test.cpp 
  test/File_test.cpp 
)

source_group("test" FILES ${TEST_SOURCES})

set(TEST_UTIL_SOURCES
  test/StorageInMemory.hpp 
  test/StorageInMemory.cpp 
  test/SparseStorage.hpp 
  test/SparseStorage.cpp 
)

source_group("util" FILES ${TEST_UTIL_SOURCES})

set(GTEST_SOURCES
  3rdparty/gtest/src/gtest_main.cc 
  3rdparty/gtest/src/gtest-all.cc 
)

source_group("gtest" FILES ${GTEST_SOURCES})

add_executable(f2f_unittest
  ${GTEST_SOURCES}
  ${TEST_SOURCES}
  ${TEST_UTIL_SOURCES}

  src/BlockStorage.hpp 
  src/BlockStorage.cpp 
  src/FileBlocks.hpp 
  src/FileBlocks.cpp 
  src/File.hpp 
  src/File.cpp 
  src/Directory.hpp 
  src/Directory.cpp 
)

target_include_directories(f2f_unittest
  PRIVATE 3rdparty/gtest/include
  PRIVATE 3rdparty/gtest
  PRIVATE src
)

set(API_TEST_SOURCES
  test/FileSystem_test.cpp 
  test/FileSystem_load_test.cpp 
)

source_group("test" FILES ${API_TEST_SOURCES})

add_executable(f2f_API_test
  ${GTEST_SOURCES}
  ${API_TEST_SOURCES}
  ${TEST_UTIL_SOURCES}
)

target_include_directories(f2f_API_test
  PRIVATE 3rdparty/gtest/include
  PRIVATE 3rdparty/gtest
)

target_link_libraries(f2f_API_test
  PRIVATE f2f
)