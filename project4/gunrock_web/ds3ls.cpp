#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t &a, const dir_ent_t &b)
{
  return std::strcmp(a.name, b.name) < 0;
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cerr << argv[0] << ": diskImageFile directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
    return 1;
  }

  // parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string directory = string(argv[2]);

  // Lookup the directory/file
  int inodeNumber = fileSystem->lookup(0, directory);
  if (inodeNumber < 0)
  {
    cerr << "Directory not found: " << directory << endl;
    return 1;
  }

  // Retrieve inode information
  inode_t inode;
  int statResult = fileSystem->stat(inodeNumber, &inode);
  if (statResult != 0)
  {
    cerr << "Directory not found" << endl;
    return 1;
  }

  // Handle directory and file cases
  if (inode.type == UFS_DIRECTORY)
  {
    // Read directory entries
    vector<char> buffer(inode.size);
    int readBytes = fileSystem->read(inodeNumber, buffer.data(), inode.size);
    if (readBytes < 0)
    {
      cerr << "Directory not found" << endl;
      return 1;
    }

    // Collect directory entries
    vector<dir_ent_t> entries;
    int offset = 0;
    while (offset < readBytes)
    {
      dir_ent_t *entry = reinterpret_cast<dir_ent_t *>(buffer.data() + offset);
      if (entry->inum != -1)
      {
        entries.push_back(*entry);
      }
      offset += sizeof(dir_ent_t);
    }

    // Sort entries by name
    sort(entries.begin(), entries.end(), compareByName);

    // Print entries
    for (const auto &entry : entries)
    {
      cout << entry.inum << "\t" << entry.name << endl;
    }
  }
  else
  {
    // Print file information
    cout << inodeNumber << "\t" << directory << endl;
  }

  return 0;
}
