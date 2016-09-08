#include <gtest/gtest.h>
#include <random>
#include <memory>

#include "BlockStorage.hpp"
#include "StorageInMemory.hpp"

struct BlockAddressLess
{
  bool operator()(f2f::BlockAddress lhs, f2f::BlockAddress rhs)
  {
    return lhs.index() < rhs.index();
  }
};

TEST(BlockStorage, T1)
{
  f2f::StorageInMemory storage(f2f::OpenMode::read_write);
  {
    f2f::BlockStorage blockStorage(storage, true);

    std::set<f2f::BlockAddress, BlockAddressLess> allocated;
    blockStorage.allocateBlocks(1, [&allocated](f2f::BlockAddress const & block)
    {
      EXPECT_TRUE(allocated.insert(block).second);
    });
    EXPECT_EQ(1, allocated.size());
    blockStorage.releaseBlocks(allocated.begin()->index(), 1);
  }
  EXPECT_EQ(0, storage.size());
  {
    // Reopening storage
    f2f::BlockStorage blockStorage(storage);
  }
}

TEST(BlockStorage, T2)
{
  f2f::StorageInMemory storage(f2f::OpenMode::read_write);
  {
    f2f::BlockStorage blockStorage(storage, true);

    std::set<f2f::BlockAddress, BlockAddressLess> allocated;
    blockStorage.allocateBlocks(30000, [&allocated](f2f::BlockAddress const & block)
    {
      EXPECT_TRUE(allocated.insert(block).second);
    });
    for(auto block: allocated)
      blockStorage.releaseBlocks(block.index(), 1);
  }
  EXPECT_EQ(0, storage.size());
  {
    // Reopening storage
    f2f::BlockStorage blockStorage(storage);
  }
}

TEST(BlockStorage, T3)
{
  f2f::StorageInMemory storage(f2f::OpenMode::read_write);
  {
    f2f::BlockStorage blockStorage(storage, true);

    std::set<f2f::BlockAddress, BlockAddressLess> allocated;
    blockStorage.allocateBlocks(f2f::format::OccupancyBlock::BitmapItemsCount, [&allocated](f2f::BlockAddress const & block)
    {
      EXPECT_TRUE(allocated.insert(block).second);
    });
    for(int i=0; i<30000; ++i)
    {
      blockStorage.allocateBlocks(1, [&allocated](f2f::BlockAddress const & block)
      {
        EXPECT_TRUE(allocated.insert(block).second);
      });
      std::vector<f2f::BlockAddress> checkAllocated;
      checkAllocated.reserve(allocated.size());
      blockStorage.enumerateAllocatedBlocks([&checkAllocated](f2f::BlockAddress const & block)
      {
        checkAllocated.push_back(block);
      });
      if (!std::equal(allocated.begin(), allocated.end(), checkAllocated.begin(), checkAllocated.end()))
      {
        std::cout << i << std::endl;
        GTEST_FAIL();
      }
    }
    for (auto block : allocated)
      blockStorage.releaseBlocks(block.index(), 1);
  }
}


TEST(BlockStorage, Random_Slow)
{
  f2f::StorageInMemory storage(f2f::OpenMode::read_write);

  std::set<f2f::BlockAddress, BlockAddressLess> allocated;
  std::unique_ptr<f2f::BlockStorage> blockStorage(new f2f::BlockStorage(storage, true));
  
  std::minstd_rand random_engine;
  std::uniform_int_distribution<int> uniform_dist1(1, 6);
  std::uniform_int_distribution<int> uniform_dist2(0, 100);

  for(int i = 0; i < 1000000; ++i)
  {
    if (i % 10000 == 0)
      std::cout << i << std::endl;
    if (uniform_dist2(random_engine) == 0)
    {
      // Reload BlockStorage
      blockStorage.reset(new f2f::BlockStorage(storage));
    }
    if (uniform_dist1(random_engine) < 3 && !allocated.empty())
    {
      std::uniform_int_distribution<size_t> uniform_dist3(0, allocated.size() - 1);
      auto it = allocated.begin();
      std::advance(it, uniform_dist3(random_engine));
      auto block = it->index();
      auto endIt = it;
      ++endIt;
      int nBlocksToRelease = 1;
      for(int maxBlocks = uniform_dist1(random_engine); nBlocksToRelease < maxBlocks; ++nBlocksToRelease, ++endIt)
      {
        if (endIt == allocated.end() || endIt->index() != block + nBlocksToRelease)
          break;
      }
      
      blockStorage->releaseBlocks(block, nBlocksToRelease);
      allocated.erase(it, endIt);

      if (i%100 == 0)
      {
        std::vector<f2f::BlockAddress> checkAllocated;
        checkAllocated.reserve(allocated.size());
        blockStorage->enumerateAllocatedBlocks([&checkAllocated](f2f::BlockAddress const & block)
        {
          checkAllocated.push_back(block);
        });
        EXPECT_TRUE(std::equal(allocated.begin(), allocated.end(), checkAllocated.begin(), checkAllocated.end()));
      }
    }
    else
    {
      //auto saved_data = storage.data();
      blockStorage->allocateBlocks(uniform_dist1(random_engine), [&allocated](f2f::BlockAddress const & block)
      {
        EXPECT_TRUE(allocated.insert(block).second);
      });

      if (i % 100 == 0)
      {
        std::vector<f2f::BlockAddress> checkAllocated;
        checkAllocated.reserve(allocated.size());
        blockStorage->enumerateAllocatedBlocks([&checkAllocated](f2f::BlockAddress const & block)
        {
          checkAllocated.push_back(block);
        });
        EXPECT_TRUE(std::equal(allocated.begin(), allocated.end(), checkAllocated.begin(), checkAllocated.end()));
      }
    }
  }
  for(auto block: allocated)
  {
    blockStorage->releaseBlocks(block.index(), 1);
  }
  EXPECT_EQ(0, storage.size());
}
