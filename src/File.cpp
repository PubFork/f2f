#include "File.hpp"
#include "util/Assert.hpp"
#include "util/StorageT.hpp"
#include "util/FloorDiv.hpp"

namespace f2f
{

File::File(BlockStorage & blockStorage)
  : m_storage(blockStorage.storage())
  , m_blockStorage(blockStorage)
  , m_openMode(OpenMode::ReadWrite)
  , m_inode()
  , m_fileBlocks(blockStorage, m_inode, m_inodeTreeRootIsDirty, true)
  , m_position(0)
{
  m_inodeAddress = m_blockStorage.allocateBlock();
  memset(&m_inode, 0, sizeof(m_inode));
  util::writeT(m_storage, m_inodeAddress, m_inode);
}

File::File(BlockStorage & blockStorage, BlockAddress const & inodeAddress, OpenMode openMode)
  : m_storage(blockStorage.storage())
  , m_blockStorage(blockStorage)
  , m_openMode(openMode)
  , m_inodeAddress(inodeAddress)
  , m_fileBlocks(blockStorage, m_inode, m_inodeTreeRootIsDirty)
  , m_position(0)
{
  util::readT(m_storage, inodeAddress, m_inode);

  uint64_t blocksSize = m_inode.blocksCount * format::AddressableBlockSize;
  F2F_FORMAT_ASSERT(blocksSize >= m_inode.fileSize);
  F2F_FORMAT_ASSERT(blocksSize - m_inode.fileSize < format::AddressableBlockSize);
}

uint64_t File::size() const
{
  return m_inode.fileSize;
}

void File::remove()
{
  m_fileBlocks.truncate(0);
  m_blockStorage.releaseBlocks(m_inodeAddress, 1);
}

void File::seek(uint64_t position)
{
  m_position = position;
}

void File::read(size_t & inOutSize, void * buffer)
{
  size_t const availableSize = size_t(std::min(uint64_t(inOutSize), m_inode.fileSize - m_position));
  inOutSize = 0;
  if (availableSize == 0)
    return;

  processData(availableSize,
    [this, &buffer](uint64_t offset, unsigned size){
      m_storage.read(offset, size, buffer);
      reinterpret_cast<char *&>(buffer) += size;
    });

  inOutSize = size_t(availableSize);
}

void File::write(size_t size, void const * buffer)
{
  if (m_openMode == OpenMode::ReadOnly)
    throw FileSystemError(ErrorCode::OperationRequiresWriteAccess, "Can't write: file is opened as read-only");
  if (size > std::numeric_limits<uint64_t>::max() - m_position)
    throw FileSystemError(ErrorCode::StorageLimitReached, "File size limit reached");
  if (size == 0)
    return;
  if (m_position + size > m_inode.fileSize)
  {
    auto prevFileSize = m_inode.fileSize;
    auto prevBlocksCount = m_inode.blocksCount;
    m_inode.blocksCount = (m_position + size + format::AddressableBlockSize - 1) / format::AddressableBlockSize;
    if (m_inode.blocksCount > prevBlocksCount)
      m_fileBlocks.append(m_inode.blocksCount - prevBlocksCount);
    m_inode.fileSize = m_position + size;
    util::writeT(m_storage, m_inodeAddress, m_inode);
    if (prevFileSize < m_position)
    {
      // Zeroing unfilled parts of added space
      auto savedPosition = m_position;
      m_position = prevFileSize;
      processData(savedPosition - prevFileSize, 
        [this](uint64_t offset, unsigned size){
          static const char ZeroBuffer[8096] = {};
          while (size > 0)
          {
            unsigned chunkSize = std::min(unsigned(sizeof(ZeroBuffer)), size);
            m_storage.write(offset, chunkSize, ZeroBuffer);
            size -= chunkSize;
            offset += chunkSize;
          }
        });
      F2F_ASSERT(m_position == savedPosition);
    }
  }

  processData(size, 
    [this, &buffer](uint64_t offset, unsigned size){
      m_storage.write(offset, size, buffer);
      reinterpret_cast<const char *&>(buffer) += size;
    });
}

void File::processData(size_t size, std::function<void (uint64_t, unsigned)> const & processFunc)
{
  uint64_t remainingBytes = size;
  uint64_t const blockIndex = m_position / format::AddressableBlockSize;
  unsigned skipFromStart = unsigned(m_position - blockIndex * format::AddressableBlockSize);
  for (m_fileBlocks.seek(blockIndex);;
    m_fileBlocks.moveToNextRange())
  {
    F2F_ASSERT(!m_fileBlocks.eof());
    FileBlocks::OffsetAndSize offsetAndSize = m_fileBlocks.currentRange();
    auto absoluteAddress = offsetAndSize.first.absoluteAddress(); // Convert blocks to bytes
    unsigned bytesToReadFromRange = offsetAndSize.second * format::AddressableBlockSize;
    if (skipFromStart > 0)
    {
      absoluteAddress += skipFromStart;
      bytesToReadFromRange -= skipFromStart;
      skipFromStart = 0;
    }
    if (bytesToReadFromRange > remainingBytes)
      bytesToReadFromRange = unsigned(remainingBytes);
    processFunc(absoluteAddress, bytesToReadFromRange);
    remainingBytes -= bytesToReadFromRange;
    if (remainingBytes == 0)
      break;
  }
  m_position += size;
}

void File::truncate()
{
  if (m_inode.fileSize > m_position)
  {
    auto prevBlocksCount = m_inode.blocksCount;
    m_inode.blocksCount = util::FloorDiv(m_position, format::AddressableBlockSize);
    if (m_inode.blocksCount != prevBlocksCount)
      m_fileBlocks.truncate(m_inode.blocksCount);
    m_inode.fileSize = m_position;
    util::writeT(m_storage, m_inodeAddress, m_inode);
  }
}

void File::check() const
{
  m_fileBlocks.check();
}

}