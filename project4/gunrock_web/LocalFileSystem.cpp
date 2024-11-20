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
  char buffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int block_num = 0; block_num < super->inode_bitmap_len; block_num++)
  {
    disk->readBlock(super->inode_bitmap_addr + block_num, inodeBitmap + (block_num * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int block_num = 0; block_num < super->inode_bitmap_len; block_num++)
  {
    disk->writeBlock(super->inode_bitmap_addr + block_num, inodeBitmap + (block_num * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int block_num = 0; block_num < super->data_bitmap_len; block_num++)
  {
    disk->readBlock(super->data_bitmap_addr + block_num, dataBitmap + (block_num * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int block_num = 0; block_num < super->data_bitmap_len; block_num++)
  {
    disk->writeBlock(super->data_bitmap_addr + block_num, dataBitmap + (block_num * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
  int inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);
  for (int block_num = 0; block_num < super->inode_region_len; block_num++)
  {
    disk->readBlock(super->inode_region_addr + block_num, inodes + inodes_per_block * block_num);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
  int inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);
  for (int block_num = 0; block_num < super->inode_region_len; block_num++)
  {
    disk->writeBlock(super->inode_region_addr + block_num, inodes + inodes_per_block * block_num);
  }
}

int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
  super_t super;
  readSuperBlock(&super);

  // Get the parent inode
  inode_t parentinode;
  int statResult = this->stat(parentInodeNumber, &parentinode);
  if (statResult != 0)
  {
    return -EINVALIDINODE; // Return error from stat if it fails
  }

  // Check if the parent inode is a directory
  if (parentinode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Read the contents of the parent inode
  char buffer[parentinode.size];
  int readBytes = this->read(parentInodeNumber, buffer, parentinode.size);
  if (readBytes < 0)
  {
    return -EINVALIDINODE; // Failed to read
  }

  int offset = 0;
  // Iterate through the directory entries
  while (offset < readBytes)
  {
    dir_ent_t *entry = reinterpret_cast<dir_ent_t *>(buffer + offset);
    // Check if the entry is valid and matches the name
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
  readSuperBlock(&super); // Read for layout info

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int blockNumber = super.inode_region_addr + (inodeNumber / inodesPerBlock);
  int offsetInBlock = (inodeNumber % inodesPerBlock) * sizeof(inode_t);

  vector<unsigned char> blockBuffer(UFS_BLOCK_SIZE);

  this->disk->readBlock(blockNumber, blockBuffer.data());

  memcpy(inode, blockBuffer.data() + offsetInBlock, sizeof(inode_t));

  if (inode->type != UFS_DIRECTORY && inode->type != UFS_REGULAR_FILE)
  {
    return -EINVALIDINODE;
  }

  return 0;
}
int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  super_t super;
  readSuperBlock(&super);

  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);

  // Calculate the block number and offset within the block for the inode
  int blockNumber = super.inode_region_addr + (inodeNumber / inodesPerBlock);
  int offsetInBlock = (inodeNumber % inodesPerBlock) * sizeof(inode_t);

  // Read the block containing the inode
  vector<unsigned char> blockBuffer(UFS_BLOCK_SIZE);
  this->disk->readBlock(blockNumber, blockBuffer.data());

  // Copy the inode data from the block buffer
  inode_t inode;
  memcpy(&inode, blockBuffer.data() + offsetInBlock, sizeof(inode_t));

  // Check if the inode is valid
  if (inode.type != UFS_DIRECTORY && inode.type != UFS_REGULAR_FILE)
  {
    return -EINVALIDINODE;
  }

  // Read the data blocks associated with the inode
  int bytesRead = 0;
  int bytesToRead = min(size, inode.size);
  int blockIndex = 0;

  while (bytesRead < bytesToRead && blockIndex < DIRECT_PTRS)
  {
    if (inode.direct[blockIndex] == 0)
    {
      break; // No more data blocks
    }

    // Read the data block
    this->disk->readBlock(inode.direct[blockIndex], blockBuffer.data());

    // Calculate the number of bytes to copy from this block
    int bytesInBlock = min(UFS_BLOCK_SIZE, bytesToRead - bytesRead);

    // Copy the data from the block buffer to the output buffer
    memcpy(static_cast<char *>(buffer) + bytesRead, blockBuffer.data(), bytesInBlock);

    bytesRead += bytesInBlock;
    blockIndex++;
  }

  return bytesRead;
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
