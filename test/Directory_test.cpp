#include <gtest/gtest.h>

#include <random>
#include "Directory.hpp"
#include "StorageInMemory.hpp"
#include "util/FNVHash.hpp"
#include "util/StorageT.hpp"

namespace
{
  std::string CreateRandomString(std::minstd_rand & random_engine)
  {
    std::uniform_int_distribution<> length_dist(1, f2f::format::MaxFileNameSize);
    std::uniform_int_distribution<> char_dist(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

    int len = length_dist(random_engine);
    std::string res;
    res.reserve(len);
    for(; len > 0; --len)
      res += std::string(1, char_dist(random_engine));
    return res;
  }

  std::pair<std::string, std::string> const HashCollisions[] = {
    {"costarring", "liquid"},
    {"declinate", "macallums"},
    {"altarage", "zinke"},
    {"altarages", "zinkes"}
  };
}

TEST(Directory, CheckCollisionPairs)
{
  for(auto const & p: HashCollisions)
    EXPECT_EQ(
      f2f::util::HashFNV1a_32(p.first.data(), p.first.data() + p.first.size()), 
      f2f::util::HashFNV1a_32(p.second.data(), p.second.data() + p.second.size()));
}

TEST(Directory, T1)
{
  std::minstd_rand random_engine;

  for(int repeat = 0; repeat < 100; ++repeat)
  {
    StorageInMemory storage;
    std::unique_ptr<f2f::BlockStorage> blockStorage(new f2f::BlockStorage(storage, true));
    std::unique_ptr<f2f::Directory> directory(new f2f::Directory(*blockStorage, f2f::Directory::NoParentDirectory));
  
    {
      f2f::Directory::Iterator it(*directory);
      EXPECT_TRUE(it.eof());
    }

    std::uniform_int_distribution<> collision_dist(0, 100'000);
    std::map<std::string, uint64_t> items;

    storage.data().reserve(500'000'000);
    auto original_storage_size = storage.data().size();

    for(int i = 0; i < 500'000; ++i)
    {
      if (i%10000 == 0)
        std::cout << i << std::endl;
      std::string name;
      auto collision_val = collision_dist(random_engine);
      if (collision_val < (std::end(HashCollisions) - std::begin(HashCollisions)) * 2)
      {
        if (collision_val%2 == 0)
          name = HashCollisions[collision_val/2].first;
        else
          name = HashCollisions[collision_val/2].second;
      }
      else
        name = CreateRandomString(random_engine);

      auto ins = items.insert(std::make_pair(name, i));
      if (ins.second)
        directory->addFile(f2f::BlockAddress::fromBlockIndex(i), f2f::FileType::Regular, name);

      if (i%10000 == 9999)
        directory->check();

      if (collision_dist(random_engine) < 100)
      {
        auto inodeIndex = directory->inodeAddress();
        directory.reset();

        if (collision_dist(random_engine) < 50'000)
          blockStorage.reset(new f2f::BlockStorage(storage));

        directory.reset(new f2f::Directory(*blockStorage, inodeIndex));
      }

      if (collision_dist(random_engine) == 1)
      {
        std::cout << "Checking directory contents" << std::endl;
        std::map<std::string, uint64_t> listed_items;
        for(f2f::Directory::Iterator it(*directory); !it.eof(); it.moveNext())
        {
          EXPECT_EQ(f2f::FileType::Regular, it.currentFileType());
          EXPECT_TRUE(listed_items.insert(std::make_pair(it.currentName(), it.currentInode().index())).second);
        }
        EXPECT_TRUE(items == listed_items);
      }
    }

    for(auto const & item: items)
    {
      auto res = directory->searchFile(item.first);
      ASSERT_TRUE(res);
      EXPECT_EQ(item.second, res->first.index());
    }
  }
  /*directory.clear();
  EXPECT_EQ(original_storage_size, storage.data().size());*/
}
