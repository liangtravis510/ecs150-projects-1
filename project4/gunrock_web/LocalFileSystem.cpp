#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

LocalFileSystem::LocalFileSystem(Disk *disk)
{
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super)
{
  vector<unsigned char> buffer(UFS_BLOCK_SIZE);
  this->disk->readBlock(0, buffer.data());
  memcpy(super, buffer.data(), sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int i = 0; i < super->inode_bitmap_len; i++)
  {
    int InodeBitMapBlockNumber = super->inode_bitmap_addr + i;
    // offset = i * UFS_BLOCK_SIZE
    this->disk->readBlock(InodeBitMapBlockNumber, inodeBitmap + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int i = 0; i < super->inode_bitmap_len; i++)
  {
    int InodeBitMapBlockNumber = super->inode_bitmap_addr + i;
    this->disk->writeBlock(InodeBitMapBlockNumber, inodeBitmap + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int i = 0; i < super->data_bitmap_len; i++)
  {
    int DataBitMapBlockNumber = super->data_bitmap_addr + i;
    this->disk->readBlock(DataBitMapBlockNumber, dataBitmap + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int i = 0; i < super->data_bitmap_len; i++)
  {
    int DataBitMapBlockNumber = super->data_bitmap_addr + i;
    this->disk->writeBlock(DataBitMapBlockNumber, dataBitmap + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
  for (int i = 0; i < super->inode_region_len; i++)
  {
    int InodeRegionBlockNumber = super->inode_region_addr + i;
    this->disk->readBlock(InodeRegionBlockNumber, inodes + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
  for (int i = 0; i < super->inode_region_len; i++)
  {
    int InodeRegionBlockNumber = super->inode_region_addr + i;
    this->disk->writeBlock(InodeRegionBlockNumber, inodes + i * UFS_BLOCK_SIZE);
  }
}

int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
  inode_t parentInode;
  int stat_result = this->stat(parentInodeNumber, &parentInode);

  if (stat_result != 0)
  {
    return -EINVALIDINODE;
  }

  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDTYPE;
  }

  // Allocate buffer to read directory entries
  vector<char> buffer(parentInode.size);
  int readBytes = this->read(parentInodeNumber, buffer.data(), parentInode.size);
  if (readBytes < 0)
  {
    return -EINVALIDINODE; // Failed to read
  }

  // Parse directory entries
  int offset = 0;
  while (offset < readBytes)
  {
    dir_ent_t *entry = reinterpret_cast<dir_ent_t *>(buffer.data() + offset);
    if (entry->inum != -1 && strcmp(entry->name, name.c_str()) == 0)
    {
      return entry->inum; // Found the entry, return the inode number
    }
    offset += sizeof(dir_ent_t); // Move to the next entry
  }

  return -ENOTFOUND; // Entry not found
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode)
{
  super_t super;
  readSuperBlock(&super);

  // Check if the inode number is valid
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  // Calculate the block number and offset within the block
  int blockNumber = inodeNumber / (UFS_BLOCK_SIZE / sizeof(inode_t));
  int offset = (inodeNumber % (UFS_BLOCK_SIZE / sizeof(inode_t))) * sizeof(inode_t);

  // Read the block containing the inode
  vector<unsigned char> buffer(UFS_BLOCK_SIZE);
  disk->readBlock(super.inode_region_addr + blockNumber, buffer.data());

  // Copy the inode data from the buffer
  memcpy(inode, buffer.data() + offset, sizeof(inode_t));

  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  inode_t inode;
  int ret = stat(inodeNumber, &inode);
  if (ret != 0)
  {
    return ret;
  }

  if (size < 0 || size > MAX_FILE_SIZE)
  {
    return -EINVALIDSIZE;
  }

  if (size > inode.size)
  {
    size = inode.size;
  }

  int bytesRead = 0;
  int blockNumber = 0;
  std::vector<unsigned char> blockBuffer(UFS_BLOCK_SIZE);

  while (bytesRead < size)
  {
    disk->readBlock(inode.direct[blockNumber], blockBuffer.data());

    int bytesToRead = std::min(size - bytesRead, UFS_BLOCK_SIZE);
    memcpy((char *)buffer + bytesRead, blockBuffer.data(), bytesToRead);

    bytesRead += bytesToRead;
    blockNumber++;
  }

  return size;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name)
{
  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size)
{
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
  return 0;
}
