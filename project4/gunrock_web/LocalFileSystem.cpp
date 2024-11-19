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
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
}

int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
  return 0;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode)
{
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  return 0;
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
