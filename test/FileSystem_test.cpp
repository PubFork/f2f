#include <gtest/gtest.h>
#include "f2f/FileSystem.hpp"
#include "StorageInMemory.hpp"

TEST(FileSystem, Basic)
{
  std::string const testString("123454321");
  f2f::FileDescriptor file2;
  {
    f2f::FileSystem fs(std::unique_ptr<f2f::IStorage>(new StorageInMemory(f2f::OpenMode::ReadWrite)), true);
    fs.createDirectory("dir1");
    fs.createDirectory("dir2");
    {
      auto file = fs.open("dir1/123.bin", f2f::OpenMode::ReadWrite);
      file.write(testString.size(), testString.data());
    }
    file2 = fs.open("/.././dir2/./../dir1/123.bin", f2f::OpenMode::ReadOnly);
  }
  {
    // Access file descriptor after FileSystem reference was released
    std::string rd(testString.size(), ' ');
    size_t size = testString.size();
    file2.read(size, &rd[0]);
    EXPECT_EQ(testString.size(), size);
    EXPECT_EQ(testString, rd);
  }
}

TEST(FileSystem, DeleteOpenedFile)
{
  std::string const testString("123454321");
  {
    f2f::FileSystem fs(std::unique_ptr<f2f::IStorage>(new StorageInMemory(f2f::OpenMode::ReadWrite)), true);
    fs.createDirectory("dir1");
    fs.createDirectory("dir2");
    {
      auto file = fs.open("dir1/123.bin", f2f::OpenMode::ReadWrite);
      file.write(testString.size(), testString.data());
    }
    
    {
      auto file = fs.open("/.././dir2/./../dir1/123.bin", f2f::OpenMode::ReadOnly);
      fs.remove("/.././dir2/./../dir1///123.bin");
      EXPECT_FALSE(fs.open("dir1/123.bin", f2f::OpenMode::ReadOnly).isOpen());
      std::string rd(testString.size(), ' ');
      size_t size = testString.size();
      file.read(size, &rd[0]);
      EXPECT_EQ(testString.size(), size);
      EXPECT_EQ(testString, rd);
    }
  }
}