#include <gtest/gtest.h>
#include "f2f/FileSystem.hpp"
#include "StorageInMemory.hpp"

TEST(FileSystem, Basic)
{
  f2f::FileSystem fs(std::unique_ptr<f2f::IStorage>(new f2f::StorageInMemory(f2f::OpenMode::ReadWrite)), true);
  fs.createDirectory("dir1");
  fs.createDirectory("dir2");
  std::string const testString("123454321");
  {
    auto file = fs.open("dir1/123.bin", f2f::OpenMode::ReadWrite);
    file.write(testString.size(), testString.data());
  }
  {
    auto file = fs.open("/.././dir2/./../dir1/123.bin",f2f::OpenMode::ReadOnly);
    std::string rd(testString.size(), ' ');
    size_t size = testString.size();
    file.read(size, &rd[0]);
    EXPECT_EQ(testString.size(), size);
    EXPECT_EQ(testString, rd);
  }
}