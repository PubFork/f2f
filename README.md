#f2f Library 0.8#

C++ library primarily aimed as file-system-in-file implementation.
But it also may be used to provide file system service on different kinds of storage
(see `f2f::IStorage` interface).

##Features##

 * Up to petabytes storage size
 * No limitation on file size or number
 * No limitation on directory size
 * Easy to use C++ API

##Requirements##

Library is written on portable C++11 and uses Boost library (>= 1.55).
f2f was tested with these compilers:

 * Visual Studio 2015
 * GCC 4.9 (-std=c++14)

##Storage format##

Small header precedes storage data:

Storage Header | Block storage
---------------|------------
`struct StorageHeader` | ...

Block storage is a organized as a tree of *occupancy groups*.
Each *occupancy group* contains information about free and occupied subgroups.
This information is stored as a *bitmap* - block, in which each bit is `1`
if corresponding subgroup is fully occupied and `0` if it has free blocks.

Level *i-1* Group 0 | Level *i* Bitmap block | Level *i-1* Group 1 | ... | Level *i-1* Group 8095 |
--------------------|------------------------|---------------------|-----|---------------------|
...                 |1Kb                     |...                  |...  |                      |

*Occupancy groups* of level 0 has different format:

Level *0* Bitmap block|Data block 0|Data block 1|...|Data block 8095|
----------------------|------------|------------|---|---------------|
1Kb                   | 1Kb        | 1Kb        |...| 1Kb           |

Data blocks are referenced by continuous *index* numbers throughout library. 
Bitmap blocks aren't referenced outside the `f2f::BlockStorage` class.

Only minimal required number of blocks is reserved in storage.
*Occupancy groups* may be truncated after whole data block.

##Files##

Each file is identified by index of its *inode* block. 
*inode* block (see `f2f::format::FileInode`) 
contains references to ranges of file data blocks and some metadata. 
These references may be organized as *B+tree* scattered on several blocks
(see `f2f::format::BlockRangesInternalNode` and `f2f::format::BlockRangesLeafNode`
for format of tree nodes).

##Directories##

Directory contains references to files and other directories together with their names.
Each directory is identified by index of its *inode* block. 
*inode* block (see `f2f::format::DirectoryInode`) 
contains records of directory entries (`f2f::format::DirectoryTreeLeafItem`)
or indirect references organized as *B+tree* with name hash used as key
(see `f2f::format::DirectoryTreeInternalNode` and `f2f::format::DirectoryTreeLeaf`
for format of tree nodes).
