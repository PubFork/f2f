#include <gtest/gtest.h>
#include "f2f/FileSystem.hpp"
#include "f2f/FileStorage.hpp"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <random>

namespace fs = boost::filesystem;

#if BOOST_VERSION <= 105500
namespace boost { namespace filesystem {
inline directory_iterator begin(directory_iterator const & it)
{ return it; }
inline directory_iterator end(directory_iterator const &)
{ return directory_iterator(); }
}}
#endif

namespace
{

std::string CreateRandomName(std::minstd_rand & random_engine)
{
  static const char NameChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";

  std::uniform_int_distribution<> length_dist(1, 16); // 255 is common limit for disk file systems
  std::uniform_int_distribution<> char_dist(0, sizeof(NameChars) - 2);

  int len = length_dist(random_engine);
  std::string res;
  res.reserve(len);
  for (; len > 0; --len)
    res += std::string(1, NameChars[char_dist(random_engine)]);
  return res;
}

fs::path FindRandomDir(std::minstd_rand & random_engine, fs::path currentDir)
{
  while (true)
  {
    std::vector<fs::path> subDirs;
    for (auto const & entry : fs::directory_iterator(currentDir))
    {
      if (fs::is_directory(entry.status()))
        subDirs.push_back(entry.path());
    }
    std::uniform_int_distribution<> subdir_dist(0, subDirs.size());
    auto subdir = subdir_dist(random_engine);
    if (subdir == subDirs.size())
      break;
    else
      currentDir = subDirs[subdir];
  }
  return currentDir;
}

fs::path FindRandomPath(std::minstd_rand & random_engine, fs::path currentDir, bool & isDir)
{
  bool root = true;
  while (true)
  {
    std::vector<std::pair<fs::path, bool>> entries;
    for (auto const & entry : fs::directory_iterator(currentDir))
    {
      entries.push_back(std::make_pair(entry.path(), fs::is_directory(entry.status())));
    }
    if (root && entries.empty())
      return fs::path();
    std::uniform_int_distribution<> entries_dist(0, !root ? entries.size() : (entries.size() - 1));
    auto entry = entries_dist(random_engine);
    if (entry == entries.size())
    {
      isDir = true;
      return currentDir;
    }
    else
      if (entries[entry].second)
        currentDir = entries[entry].first;
      else
      {
        isDir = entries[entry].second;
        return entries[entry].first;
      }
    root = false;
  }
}

void GenerateRandomData(std::minstd_rand & random_engine, char * buffer, size_t size)
{
  while(size > 0)
  {
    unsigned rnd = random_engine();
    char const * rnd_char = reinterpret_cast<char const *>(&rnd);
    for (int i = 0; i < sizeof(rnd) && size > 0; ++i)
    {
      *(buffer++) = rnd_char[i];
      --size;
    }
  }
}

fs::path RemovePathRoot(fs::path const & src)
{
  fs::path out;
  for(auto it = ++src.begin(); it != src.end(); ++it)
    out /= *it;
  return out;
}

void RandomWriteToFiles(f2f::FileSystem & fs, fs::path const & f2f_path, fs::path const & os_path, std::ostream & log)
{
  static std::minstd_rand random_engine;
  auto f2f_file = fs.open(f2f_path.generic_string().c_str(), f2f::OpenMode::ReadWrite);
  fs::fstream os_file(os_path, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
  std::vector<char> buffer(500'000);
  for(int i = 0, num = std::uniform_int_distribution<>(1, 3)(random_engine); i < num; ++i)
  {
    uint64_t position = std::fabs(std::normal_distribution<>(0, 1'000'000)(random_engine));
    auto blockSize = std::uniform_int_distribution<>(0, buffer.size() - 1)(random_engine);
    GenerateRandomData(random_engine, buffer.data(), blockSize);
    f2f_file.seek(position);
    f2f_file.write(blockSize, buffer.data());
    os_file.seekp(position);
    os_file.write(buffer.data(), blockSize);

    log << "Write to \"" << f2f_path.generic_string() << "\" " << blockSize << " bytes at offset " << position << std::endl;
  }
}

void CompareFiles(f2f::FileSystem const & fs, fs::path const & f2f_path, fs::path const & os_path)
{
  static std::minstd_rand random_engine;
  auto f2f_file = fs.open(f2f_path.generic_string().c_str());
  fs::ifstream os_file(os_path, std::ios_base::binary);
  std::vector<char> buffer1(500'000);
  std::vector<char> buffer2(buffer1.size());
  while (true)
  {
    auto blockSize = std::uniform_int_distribution<>(0, buffer1.size() - 1)(random_engine);
    size_t f2f_read = blockSize;
    f2f_file.read(f2f_read, buffer1.data());
    os_file.read(buffer2.data(), blockSize);
    EXPECT_EQ(f2f_read, os_file.gcount());
    EXPECT_EQ(0, memcmp(buffer1.data(), buffer2.data(), f2f_read));
    if (f2f_read < blockSize)
      break;
  }
}

void CompareDirectories(f2f::FileSystem const & fs, fs::path const & f2f_dir, fs::path const & os_dir)
{
  std::map<std::string, bool> os_entries, f2f_entries; // <name, isDir>
  for(auto const & entry : fs::directory_iterator(os_dir))
  {
    EXPECT_TRUE(os_entries.insert(std::make_pair(entry.path().filename().generic_string(), fs::is_directory(entry.status()))).second);
  }

  for(auto const & entry : fs.directoryIterator(f2f_dir.generic_string().c_str()))
  {
    EXPECT_TRUE(f2f_entries.insert(std::make_pair(entry.name(), entry.type() == f2f::FileType::Directory)).second);
  }

  EXPECT_TRUE(os_entries == f2f_entries);
  for(auto const & entry: os_entries)
  {
    if (entry.second)
      CompareDirectories(fs, f2f_dir / entry.first, os_dir / entry.first);
    else
      CompareFiles(fs, f2f_dir / entry.first, os_dir / entry.first);
  }
}

}

TEST(FileSystem, LoadTest)
{
  fs::path testRootDir("f2f_LoadTest.root");
  static const char FileStorageName[] = "f2f_LoadTest.stg";

  std::ofstream log("f2f_LoadTest.log");

  fs::remove_all(testRootDir);
  fs::create_directory(testRootDir);
  fs::remove(FileStorageName);

  try
  {
    std::unique_ptr<f2f::FileSystem> fs(new f2f::FileSystem(f2f::OpenFileStorage(FileStorageName), true));

    std::vector<char> buffer(500'000);
    std::minstd_rand random_engine;
    std::uniform_int_distribution<> action_dist(0, 100);
    std::uniform_int_distribution<> check_dist(0, 100000);
    for(int i=0; i<1'000'000; ++i)
    {
      if (i % 10000 == 0)
        std::cout << "Step " << i << std::endl;

      auto action = action_dist(random_engine);
      if (action < 20)
      {
        // Create file or directory
        bool create_directory = action < 2;
        fs::path currentDir = FindRandomDir(random_engine, testRootDir);
        auto randomName = CreateRandomName(random_engine);
        fs::path currentDirF2F(RemovePathRoot(currentDir));
        if (!fs::exists(currentDir / randomName))
          // TODO: Try to create existing dirs/files and check for errors
          if (create_directory)
          {
            log << "Create directory \"" << (currentDirF2F / randomName).generic_string() << "\"" << std::endl;
            fs->createDirectory((currentDirF2F / randomName).generic_string().c_str());
            fs::create_directory(currentDir / randomName);
          }
          else
          {
            auto fileDescriptor = fs->open((currentDirF2F / randomName).generic_string().c_str(), f2f::OpenMode::ReadWrite);
            EXPECT_TRUE(fileDescriptor.isOpen());
            fs::ofstream file(currentDir / randomName, std::ios::binary);
            auto blocksNum = std::uniform_int_distribution<>(0, 10)(random_engine);
            for(int i=0; i<blocksNum; ++i)
            {
              auto blockSize = std::uniform_int_distribution<>(0, 10000)(random_engine);
              GenerateRandomData(random_engine, buffer.data(), blockSize);
              fileDescriptor.write(blockSize, buffer.data());
              file.write(buffer.data(), blockSize);
            }
            log << "Create file \"" << (currentDirF2F / randomName).generic_string() << "\" " << fileDescriptor.size() << " bytes" << std::endl;
          }
      } 
      else if (action < 33)
      {
        // Delete file or directory
        bool isDir;
        auto path = FindRandomPath(random_engine, testRootDir, isDir);
        if (!path.empty())
        {
          if (!isDir || action_dist(random_engine) < 5)
          {
            log << "Delete \"" << RemovePathRoot(path).generic_string() << "\"" << std::endl;
            fs->remove(RemovePathRoot(path).generic_string().c_str());
            if (!isDir)
              fs::remove(path);
            else
            {
              try
              {
                fs::remove_all(path);
              } catch(const fs::filesystem_error & )
              {
  #ifdef _WIN32
                // On Windows for some unknown reason "The directory is not empty" error is thrown sometimes
                // and retrying operation helps
                fs::remove_all(path);
  #else
                throw;
  #endif
              }
            }          
          }
        }
      }
      else if (action < 40)
      {
        // Write to file
        bool isDir;
        auto path = FindRandomPath(random_engine, testRootDir, isDir);
        if (!path.empty() && !isDir)
        {
          RandomWriteToFiles(*fs, RemovePathRoot(path), path, log);
        }
      }
      else if (action < 42)
      {
        // Truncate file
        bool isDir;
        auto path = FindRandomPath(random_engine, testRootDir, isDir);
        if (!path.empty() && !isDir)
        {
          auto f2f_file = fs->open(RemovePathRoot(path).generic_string().c_str(), f2f::OpenMode::ReadWrite);
          if (f2f_file.size() > 0)
          {
            auto dest_size = std::uniform_int_distribution<>(0, f2f_file.size())(random_engine);
            f2f_file.seek(dest_size);
            f2f_file.truncate();
            fs::resize_file(path, dest_size);

            log << "Truncate \"" << RemovePathRoot(path).generic_string() << "\" at " << dest_size << std::endl;
          }
        }
      }
      else if (action == 100)
      {
        // Reload storage
        log << "Reload storage" << std::endl;
        fs.reset();
        fs.reset(new f2f::FileSystem(f2f::OpenFileStorage(FileStorageName), false));
      }

      if (check_dist(random_engine) == 0)
      {
        // Perform full comparison
        std::cout << "Performing full comparison" << std::endl;
        CompareDirectories(*fs, fs::path(), testRootDir);
      }

      if (check_dist(random_engine) < 10)
      {
        // File system check
        std::cout << "Checking file system" << std::endl;
        fs->check();
      }
    }

    // Remove all
    for(auto const & entry : fs->directoryIterator(""))
    {
      fs->remove(entry.name().c_str());
    }
  }
  catch (...)
  {
    fs::remove_all(testRootDir);
    fs::remove(FileStorageName);
    throw;
  }
  fs::remove_all(testRootDir);
  fs::remove(FileStorageName);
}
