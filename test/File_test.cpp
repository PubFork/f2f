#include <gtest/gtest.h>

#include <vector>
#include <memory>
#include <random>
#include "File.hpp"
#include "StorageInMemory.hpp"
#include "SparseStorage.hpp"
#include "util/StorageT.hpp"

TEST(File, NoRemains1)
{
  StorageInMemory storage;
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

class FileRemainsTest : public ::testing::TestWithParam<std::pair<int, uint64_t>> {
};

TEST_P(FileRemainsTest, NoRemains2)
{
  const uint64_t SizeLimit   = GetParam().second;
  const size_t MaxFileSize = 5'000'000;
  const unsigned CheckPeriod = SizeLimit / MaxFileSize / 10;

  std::unique_ptr<f2f::IStorage> storage;
  if (SizeLimit < 1'000'000'000)
  {
    std::unique_ptr<StorageInMemory> mstorage(new StorageInMemory);
    mstorage->data().reserve(SizeLimit * 1.10);
    storage = std::move(mstorage);
  }
  else
  {
    storage.reset(new SparseStorage);
  }

  f2f::BlockStorage blockStorage(*storage, true);
  std::vector<std::unique_ptr<f2f::File>> files;
  for(int i=0; i<GetParam().first; ++i)
    files.push_back(std::unique_ptr<f2f::File>(new f2f::File(blockStorage)));
  std::vector<std::pair<unsigned, uint64_t>> log; // <amount written, size in blocks>
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
      log.push_back(std::make_pair(size, blockStorage.blocksCount()));
      totalSize += size;
    }
  }

  // Zeroing all files again
  for(auto const & file: files)
  {
    file->seek(0);
    for(uint64_t pos = 0; pos < file->size(); pos += buf.size())
      file->write(std::min(uint64_t(buf.size()), file->size() - pos), buf.data());
  }

  for (auto const & file : files)
    file->check();

  for(int i=0; !log.empty(); ++i)
  {
    for(auto it = files.rbegin(); it != files.rend(); ++it)
    {
      EXPECT_EQ(log.back().second, blockStorage.blocksCount());
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
    std::make_pair(1, UINT64_C(100'000'000)),
    std::make_pair(2, UINT64_C(100'000'000)),
    std::make_pair(3, UINT64_C(100'000'000)),
    std::make_pair(10, UINT64_C(100'000'000)),
    std::make_pair(3, UINT64_C(100'000'000'000'000)) // 100 TB
  )
);