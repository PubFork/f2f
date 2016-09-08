#include <gtest/gtest.h>

#include <vector>
#include <memory>
#include <random>
#include "File.hpp"
#include "StorageInMemory.hpp"
#include "util/StorageT.hpp"

TEST(File, NoRemains1)
{
  f2f::StorageInMemory storage(f2f::OpenMode::read_write);
  f2f::BlockStorage blockStorage(storage, true);
  f2f::File file1(blockStorage);
  f2f::File file2(blockStorage);
  auto storageSize = storage.data().size();
  const char buf[2000] = {};
  for(int i=0; i<100000; ++i)
  {
    file1.write(792, buf);
    file2.write(792, buf);
  }
  file1.seek(0);
  file1.truncate();
  file2.seek(0);
  file2.truncate();
  EXPECT_EQ(storageSize, storage.data().size());
}

class FileRemainsTest : public ::testing::TestWithParam<int> {
};

TEST_P(FileRemainsTest, NoRemains2)
{
  static const size_t SizeLimit   = sizeof(size_t) == 4 ? 900'000'000 : 5'000'000'000ui64;
  static const size_t MaxFileSize = sizeof(size_t) == 4 ?   3'000'000 : 5'000'000;
  static const unsigned CheckPeriod = SizeLimit / MaxFileSize / 50;

  f2f::StorageInMemory storage(f2f::OpenMode::read_write);
  storage.data().reserve(SizeLimit * 1.10);

  f2f::BlockStorage blockStorage(storage, true);
  std::vector<std::unique_ptr<f2f::File>> files;
  for(int i=0; i<GetParam(); ++i)
    files.push_back(std::unique_ptr<f2f::File>(new f2f::File(blockStorage)));
  std::vector<std::pair<unsigned, size_t>> log; // <amount written, size after>
  size_t totalSize = 0;

  std::vector<char> buf(MaxFileSize);
  std::minstd_rand random_engine;
  std::uniform_int_distribution<int> uniform_dist1(1, buf.size());
  for(int i=0; totalSize < SizeLimit; ++i)
  {
    for(auto const & file: files)
    {
      auto size = uniform_dist1(random_engine);
      file->write(size, buf.data());
      log.push_back(std::make_pair(size, storage.data().size()));
      totalSize += size;
    }
  }

  for (auto const & file : files)
    file->check();

  for(int i=0; !log.empty(); ++i)
  {
    for(auto it = files.rbegin(); it != files.rend(); ++it)
    {
      EXPECT_EQ(log.back().second, storage.data().size());
      (*it)->seek((*it)->position() - log.back().first);
      (*it)->truncate();
      if (i%CheckPeriod == 0)
        (*it)->check();
      log.pop_back();
    }
  }

  for (auto const & file : files)
    file->check();
}

INSTANTIATE_TEST_CASE_P(FileTest,
  FileRemainsTest,
  ::testing::Values(
    1,
    2,
    3,
    10
  )
);