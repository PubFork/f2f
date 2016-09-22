#include <gtest/gtest.h>
#include "f2f/FileSystem.hpp"
#include "f2f/FileStorage.hpp"
#include <random>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/dynamic_bitset.hpp>

namespace fs = boost::filesystem;

namespace
{
  struct DirectoryDef
  {
    DirectoryDef()
      : filesCount(0)
    {}

    std::string name;
    uint64_t filesCount;
  };
}

class FileSystemHugeDirectory: public ::testing::TestWithParam<int> {
};

TEST_P(FileSystemHugeDirectory, HugeDirectory)
{
  //std::cout << "Warning! This test will fill up current disk" << std::endl;
  static const char FileStorageName[] = "f2f_HugeDir.stg";

  fs::remove(FileStorageName);
  std::minstd_rand random_engine;

  try
  {
    std::vector<DirectoryDef> dirs(GetParam());
    for(int i=0; i<dirs.size(); ++i)
      dirs[i].name = "Directory" + boost::lexical_cast<std::string>(i + 1);

    {
      f2f::FileSystem fs(f2f::OpenFileStorage(FileStorageName), true);
      for(auto const & dir: dirs)
        fs.createDirectory(dir.name.c_str());

      try
      {
        //while (true)
        while (dirs.front().filesCount < 200000)
        {
          for(auto & dir: dirs)
          {
            auto sequenceSize = std::uniform_int_distribution<>(1, 5)(random_engine);
            for(int i=0; i<sequenceSize; ++i)
            {
              fs.open((dir.name + "/File" + boost::lexical_cast<std::string>(dir.filesCount)).c_str(), f2f::OpenMode::ReadWrite);
              ++dir.filesCount;
            }
          }
        }
      } 
      catch(const std::exception &)
      {
        // Disk is full
      }

      for(auto & dir: dirs)
        std::cout << dir.filesCount << " files added to the directory " << dir.name << std::endl;
    }

    std::cout << "Deleting files" << std::endl;

    {
      f2f::FileSystem fs(f2f::OpenFileStorage(FileStorageName), false);

      for(auto const & dir: dirs)
      {
        // Enumerating files in directory
        boost::dynamic_bitset<> folderBitset(dir.filesCount);
        uint64_t foundFiles = 0;
        for(auto const & entry: fs.directoryIterator(dir.name.c_str()))
        {
          EXPECT_TRUE(entry.name().substr(0, 4) == "File");
          uint64_t fileNum = boost::lexical_cast<uint64_t>(entry.name().substr(4));
          EXPECT_LT(fileNum, dir.filesCount);
          EXPECT_FALSE(folderBitset.test(fileNum));
          folderBitset.set(fileNum);
          ++foundFiles;
        }
        EXPECT_EQ(dir.filesCount, foundFiles);

        // Removing files from the directory
        while (foundFiles > 0)
        {
          int64_t fileNum = std::uniform_int_distribution<int64_t>(0, dir.filesCount - 1)(random_engine);
          if (folderBitset.test(fileNum))
          {
            folderBitset.reset(fileNum);
            fs.remove((dir.name + "/File" + boost::lexical_cast<std::string>(fileNum)).c_str());
            --foundFiles;
            if (foundFiles % 10000 == 0)
              std::cout << foundFiles << " remain in " << dir.name << std::endl;
          }
        }
      }
    }
  }
  catch (...)
  {
    fs::remove(FileStorageName);
    throw;
  }
  fs::remove(FileStorageName);
}

INSTANTIATE_TEST_CASE_P(FileSystem,
  FileSystemHugeDirectory,
  ::testing::Values(1, 3));