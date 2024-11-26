#include <iostream>
#include <string>
#include <memory>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3" << endl;
    return 1;
  }

  // Parse command line arguments
  /*
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string srcFile = string(argv[2]);
  int dstInode = stoi(argv[3]);
  */

  unique_ptr<Disk> disk = make_unique<Disk>(argv[1], UFS_BLOCK_SIZE);
  unique_ptr<LocalFileSystem> fileSystem = make_unique<LocalFileSystem>(disk.get());
  string srcFile = string(argv[2]);
  int dstInode = stoi(argv[3]);

  // Open the source file
  int srcFd = open(srcFile.c_str(), O_RDONLY);
  if (srcFd < 0)
  {
    cerr << "Failed to open file" << endl;
    return 1;
  }

  char read_buffer[UFS_BLOCK_SIZE];
  string write_buffer;
  int bytesRead = 0;
  while ((bytesRead = read(srcFd, read_buffer, UFS_BLOCK_SIZE)) > 0)
  {
    write_buffer.append(read_buffer, bytesRead);
  }
  close(srcFd);

  if (bytesRead < 0)
  {
    cerr << "Read error" << endl;
    return 1;
  }

  // Write the file to the disk
  disk->beginTransaction();
  if (fileSystem->write(dstInode, write_buffer.c_str(), write_buffer.size()) < 0)
  {
    disk->rollback();
    cerr << "Could not write to dst_file" << endl;
    return 1;
  }
  disk->commit();
  return 0;
}
