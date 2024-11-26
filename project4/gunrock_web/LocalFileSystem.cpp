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
  // If the size is invalid , return an error
  if (size < 0)
  {
    return -EINVALIDSIZE;
  }

  // Check for the existence of the inode
  super_t super;
  readSuperBlock(&super);
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

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
  /**
   * Write the contents of a file.
   *
   * Writes a buffer of size to the file, replacing any content that
   * already exists.
   *
   * Success: number of bytes written
   * Failure: -EINVALIDINODE, -EINVALIDSIZE, -EINVALIDTYPE.
   * Failure modes: invalid inodeNumber, invalid size, not a regular file
   * (because you can't write to directories).
   */
  // 1. Validate `name`
  if (name.empty() || name.length() >= DIR_ENT_NAME_SIZE)
  {
    return -EINVALIDNAME; // Invalid name length
  }

  // 2. Validate `type`
  if (type != UFS_REGULAR_FILE && type != UFS_DIRECTORY)
  {
    return -EINVALIDTYPE; // Invalid type
  }

  // 3. Read the superblock
  super_t super;
  char superBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, superBlockBuffer);
  memcpy(&super, superBlockBuffer, sizeof(super_t));

  // 4. Validate `parentInodeNumber`
  if (parentInodeNumber < 0 || parentInodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE; // Out of bounds
  }

  // Read the inode bitmap
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  for (int i = 0; i < super.inode_bitmap_len; i++)
  {
    disk->readBlock(super.inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }

  // Check if parent inode is allocated
  int byteOffset = parentInodeNumber % 8;
  int bitmapByte = parentInodeNumber / 8;
  char bitmask = 0b1 << byteOffset;
  if (!(inodeBitmap[bitmapByte] & bitmask))
  {
    return -EINVALIDINODE; // Not allocated
  }

  // Read parent inode
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int inodeBlock = super.inode_region_addr + (parentInodeNumber / inodesPerBlock);
  int inodeOffset = (parentInodeNumber % inodesPerBlock) * sizeof(inode_t);
  char inodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(inodeBlock, inodeBlockBuffer);
  inode_t parentInode;
  memcpy(&parentInode, inodeBlockBuffer + inodeOffset, sizeof(inode_t));

  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Parent is not a directory
  }

  // 5. Check for duplicate `name`
  int totalEntries = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t buffer[totalEntries];
  read(parentInodeNumber, buffer, parentInode.size);
  for (int i = 0; i < totalEntries; i++)
  {
    if (buffer[i].name == name)
    {
      return -EINVALIDTYPE; // Name exists with the wrong type
    }
  }

  // 6. Find a free inode
  int freeInodeNumber = -1;
  for (int i = 0; i < super.num_inodes; i++)
  {
    int byteIndex = i / 8;
    int bitIndex = i % 8;
    if (!(inodeBitmap[byteIndex] & (1 << bitIndex)))
    {
      freeInodeNumber = i;
      inodeBitmap[byteIndex] |= (1 << bitIndex); // Mark as allocated
      break;
    }
  }
  if (freeInodeNumber == -1)
  {
    return -ENOTENOUGHSPACE; // No free inodes
  }

  // 7. Allocate data block if creating a directory
  unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  for (int i = 0; i < super.data_bitmap_len; i++)
  {
    disk->readBlock(super.data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }

  int freeDataBlock = -1;
  if (type == UFS_DIRECTORY)
  {
    for (int i = 0; i < super.num_data; i++)
    {
      int byteIndex = i / 8;
      int bitIndex = i % 8;
      if (!(dataBitmap[byteIndex] & (1 << bitIndex)))
      {
        freeDataBlock = i;
        dataBitmap[byteIndex] |= (1 << bitIndex); // Mark as allocated
        break;
      }
    }
    if (freeDataBlock == -1)
    {
      return -ENOTENOUGHSPACE; // No free blocks
    }
  }

  // 8. Initialize the new inode
  inode_t newInode = {};
  newInode.type = type;
  newInode.size = (type == UFS_DIRECTORY) ? 2 * sizeof(dir_ent_t) : 0;
  if (type == UFS_DIRECTORY)
  {
    newInode.direct[0] = super.data_region_addr + freeDataBlock;

    // Create `.` and `..` entries
    dir_ent_t dirEntries[2];
    strcpy(dirEntries[0].name, ".");
    dirEntries[0].inum = freeInodeNumber;
    strcpy(dirEntries[1].name, "..");
    dirEntries[1].inum = parentInodeNumber;

    char dirBuffer[UFS_BLOCK_SIZE] = {0};
    memcpy(dirBuffer, dirEntries, 2 * sizeof(dir_ent_t));
    disk->writeBlock(newInode.direct[0], dirBuffer);
  }

  // Write the new inode to disk
  int newInodeBlock = super.inode_region_addr + (freeInodeNumber / inodesPerBlock);
  int newInodeOffset = (freeInodeNumber % inodesPerBlock) * sizeof(inode_t);
  char newInodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(newInodeBlock, newInodeBlockBuffer);
  memcpy(newInodeBlockBuffer + newInodeOffset, &newInode, sizeof(inode_t));
  disk->writeBlock(newInodeBlock, newInodeBlockBuffer);

  // 9. Add the new entry to the parent directory
  dir_ent_t newEntry = {};
  strcpy(newEntry.name, name.c_str());
  newEntry.inum = freeInodeNumber;

  int parentBlock = parentInode.direct[parentInode.size / UFS_BLOCK_SIZE];
  int offset = (parentInode.size % UFS_BLOCK_SIZE) / sizeof(dir_ent_t);
  char parentBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(parentBlock, parentBlockBuffer);
  memcpy(parentBlockBuffer + offset * sizeof(dir_ent_t), &newEntry, sizeof(dir_ent_t));
  disk->writeBlock(parentBlock, parentBlockBuffer);

  // Update the parent inode size
  parentInode.size += sizeof(dir_ent_t);
  memcpy(inodeBlockBuffer + inodeOffset, &parentInode, sizeof(inode_t));
  disk->writeBlock(inodeBlock, inodeBlockBuffer);

  // 10. Write updated bitmaps to disk
  for (int i = 0; i < super.inode_bitmap_len; i++)
  {
    disk->writeBlock(super.inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }
  for (int i = 0; i < super.data_bitmap_len; i++)
  {
    disk->writeBlock(super.data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }

  return freeInodeNumber;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size)
{
  // Validate the input size
  if (size < 0)
  {
    return -EINVALIDSIZE;
  }

  // Read the superblock
  super_t super;
  char superBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, superBlockBuffer);
  memcpy(&super, superBlockBuffer, sizeof(super_t));

  // Validate the inode number
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  // Read the inode
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int inodeBlock = super.inode_region_addr + (inodeNumber / inodesPerBlock);
  int inodeOffset = (inodeNumber % inodesPerBlock) * sizeof(inode_t);
  char inodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(inodeBlock, inodeBlockBuffer);
  inode_t inode;
  memcpy(&inode, inodeBlockBuffer + inodeOffset, sizeof(inode_t));

  // Validate inode type
  if (inode.type != UFS_REGULAR_FILE)
  {
    return -EINVALIDTYPE;
  }

  // Calculate the required and current blocks
  int requiredBlocks = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  int currentBlocks = (inode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  // Allocate additional blocks if needed
  char dataBitmapBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(super.data_bitmap_addr, dataBitmapBuffer);

  for (int i = currentBlocks; i < requiredBlocks; i++)
  {
    bool blockAllocated = false;
    for (int j = 0; j < super.num_data; j++)
    {
      int byteIndex = j / 8;
      int bitIndex = j % 8;

      if (!(dataBitmapBuffer[byteIndex] & (1 << bitIndex)))
      {
        // Mark the block as allocated
        dataBitmapBuffer[byteIndex] |= (1 << bitIndex);
        inode.direct[i] = super.data_region_addr + j;
        blockAllocated = true;
        break;
      }
    }

    if (!blockAllocated)
    {
      return -ENOTENOUGHSPACE; // No free blocks available
    }
  }

  // Write back the updated data bitmap
  disk->writeBlock(super.data_bitmap_addr, dataBitmapBuffer);

  // Write data to blocks
  int bytesWritten = 0;
  for (int i = 0; i < requiredBlocks; i++)
  {
    int blockNum = inode.direct[i];
    int offset = bytesWritten;
    int chunkSize = std::min(UFS_BLOCK_SIZE, size - bytesWritten);

    char blockBuffer[UFS_BLOCK_SIZE] = {0};
    memcpy(blockBuffer, static_cast<const char *>(buffer) + offset, chunkSize);
    disk->writeBlock(blockNum, blockBuffer);

    bytesWritten += chunkSize;
  }

  // Update inode size and write it back
  inode.size = size;
  memcpy(inodeBlockBuffer + inodeOffset, &inode, sizeof(inode_t));
  disk->writeBlock(inodeBlock, inodeBlockBuffer);

  return bytesWritten;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
  // 1. Validate `name`
  if (name.empty() || name.length() >= DIR_ENT_NAME_SIZE)
  {
    return -EINVALIDNAME; // Invalid name
  }
  if (name == "." || name == "..")
  {
    return -EUNLINKNOTALLOWED; // Cannot unlink "." or ".."
  }

  // 2. Read the superblock
  super_t super;
  char superBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, superBlockBuffer);
  memcpy(&super, superBlockBuffer, sizeof(super_t));

  // 3. Validate `parentInodeNumber`
  if (parentInodeNumber < 0 || parentInodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE; // Out of bounds
  }

  // Read the inode bitmap
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  for (int i = 0; i < super.inode_bitmap_len; i++)
  {
    disk->readBlock(super.inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }

  // Check if parent inode is allocated
  int byteOffset = parentInodeNumber % 8;
  int bitmapByte = parentInodeNumber / 8;
  char bitmask = 0b1 << byteOffset;
  if (!(inodeBitmap[bitmapByte] & bitmask))
  {
    return -EINVALIDINODE; // Not allocated
  }

  // Read parent inode
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int parentInodeBlock = super.inode_region_addr + (parentInodeNumber / inodesPerBlock);
  int parentInodeOffset = (parentInodeNumber % inodesPerBlock) * sizeof(inode_t);
  char parentInodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(parentInodeBlock, parentInodeBlockBuffer);
  inode_t parentInode;
  memcpy(&parentInode, parentInodeBlockBuffer + parentInodeOffset, sizeof(inode_t));

  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Parent is not a directory
  }

  // 4. Locate the target entry in the parent directory
  int totalEntries = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t dirEntries[totalEntries];
  read(parentInodeNumber, dirEntries, parentInode.size);

  int targetIndex = -1;
  for (int i = 0; i < totalEntries; i++)
  {
    if (name == dirEntries[i].name)
    {
      targetIndex = i;
      break;
    }
  }

  if (targetIndex == -1)
  {
    return -ENOTFOUND; // Entry not found
  }

  // Get the inode number of the target
  int targetInodeNumber = dirEntries[targetIndex].inum;

  // Read the target inode
  int targetInodeBlock = super.inode_region_addr + (targetInodeNumber / inodesPerBlock);
  int targetInodeOffset = (targetInodeNumber % inodesPerBlock) * sizeof(inode_t);
  char targetInodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(targetInodeBlock, targetInodeBlockBuffer);
  inode_t targetInode;
  memcpy(&targetInode, targetInodeBlockBuffer + targetInodeOffset, sizeof(inode_t));

  // 5. Handle unlinking based on type
  if (targetInode.type == UFS_DIRECTORY)
  {
    // Ensure directory is empty (only `.` and `..` should exist)
    if (targetInode.size > static_cast<int>(2 * sizeof(dir_ent_t)))
    {
      return -EDIRNOTEMPTY; // Cannot unlink a non-empty directory
    }
  }

  // Deallocate data blocks (if any)
  unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  for (int i = 0; i < super.data_bitmap_len; i++)
  {
    disk->readBlock(super.data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }

  int numBlocks = (targetInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  for (int i = 0; i < numBlocks; i++)
  {
    int blockNumber = targetInode.direct[i] - super.data_region_addr;
    int byteIndex = blockNumber / 8;
    int bitIndex = blockNumber % 8;
    dataBitmap[byteIndex] &= ~(1 << bitIndex); // Mark block as free
  }

  // Write updated data bitmap
  for (int i = 0; i < super.data_bitmap_len; i++)
  {
    disk->writeBlock(super.data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }

  // Deallocate the target inode
  int targetByteIndex = targetInodeNumber / 8;
  int targetBitIndex = targetInodeNumber % 8;
  inodeBitmap[targetByteIndex] &= ~(1 << targetBitIndex); // Mark inode as free

  // Write updated inode bitmap
  for (int i = 0; i < super.inode_bitmap_len; i++)
  {
    disk->writeBlock(super.inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }

  // 6. Remove the directory entry from the parent
  if (targetIndex < totalEntries - 1)
  {
    dirEntries[targetIndex] = dirEntries[totalEntries - 1]; // Replace with last entry
  }
  parentInode.size -= sizeof(dir_ent_t);

  // Write back the updated parent directory
  write(parentInodeNumber, dirEntries, parentInode.size);

  // Update parent inode
  memcpy(parentInodeBlockBuffer + parentInodeOffset, &parentInode, sizeof(inode_t));
  disk->writeBlock(parentInodeBlock, parentInodeBlockBuffer);

  return 0; // Success
}