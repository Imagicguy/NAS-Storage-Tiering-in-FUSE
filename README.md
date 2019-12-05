# s3-storage-tiering
## 1. Introduction

It is very common to utilize remote network filesystem for data storage as an effective, scalable, low-cost storage solution. Based on that, we built a software named RAT-FS to provide a large local disk cache for a remote network filesystem.

Business scenario: considering you have a network filesystem, such as SSH or NFS or CIFS or S3FS (mount an S3 bucket via FUSE) mounted share, and the connection to that share is rather slow. Locally, you have a sizable amount of disk space to spare, but not enough to make a complete local copy of the remote share. Then what you can do is to apply our RAT-FS, a filesystem that act between the local disk space and the remote network filesystem, to substantially speed up data access and reduce network traffic. 

When data is requested, RAT-FS first checks its local cache. If the requested data is not there, it fetches the requested data over the network and stores them in the cache. This continues until the cache is full, then the cache invalidate the least recently accessed data. Therefore, the most frequently used data will be in the local cache and won't need to be fetched from the network, but the whole share is still available.

## 2. Design

2.1 Overall architecture: 




2.3 Local Cache Directory: 

The cache is implemented as an in-memory LRU cache with two major data structures: a hashmap and a pair of doubly-linked lists working as a free list and a used list. The head of the used list is the node which was most recently accessed for reading. The tail is the least recently accessed, and the next one node for deletion. The node of those two doubly-linked lists has the location information of one single data block on cache.img, a local disk image for storing the most recently used data, allocated before running RAT-FS.



Inside cache_dir, there are different sub directories which have the same names as the remote store directory. For example, file /dog.txt in the remote directory is a directory /dog.txt in the local cache directory, which contains numbers range from 0 to ( cache.img size / block_size). Each number contains the on-disk cache.img information of that block of data. The format of is #<block_number on disk image>#<offset>#<size>.



2.2 Read, Write, Update, Remove: 

RAT-FS overwrites a couple of system calls such as getattr, read, write, readdir, opendir, etc using FUSE and pass them through to the remote network filesystem specified as the remote storage. Magic happens in the read and write functions where RAT-FS interacts with local cache directory and the mounted remote network filesystem. In RAT-FS, every file is seen by the user in mount_dir. The local cache is a write-through cache and data to read is cached. Writing data as well as creating a new file happens in the remote network filesystem. Whenever user “cat” a file, if the file name isn’t found in the cache directory, data will be fetched from the remote directory. Since the data now becomes most recently used, it will be stored on cache.img. To update a file, the first thing to do is to check whether the file is in cache_dir, if so, it should be invalidated and free the blocks of that file. Next, the new content of this file is written to the remote directory and the corresponding node in local cache is removed, then the next time user read that updated data, it will get the updated data from remote and add it to cache. 

2.5 RAT-FS Set-up: 

To use RAT-FS, there are a few steps before running the program. First, figure out how much free disk space available locally and df -h command should help. Then run dd if=/dev/zero of=cache.img bs=<cache.img size> count=1 and make a filesystem to hold the cache by doing mkfs.ext2 -m 0 -F cache.img. After mounting the remote network filesystem, you should be ready to go. To run RAT-FS, use ./fuse <cache_dir> <remote_dir> <mount_dir> <cache_size> <block_size>. Notice that the directory name cannot end with a slash ‘/’ and cache_size&block_size is measured by bytes.

## 3. Implementation

3.1 Overall implementation:

RAT-FS is implemented in C & C++ and applied FUSE (filesystem in userspace) program to provide efficient storage solutions for NFS server. Based on the design architecture, implementation of RAT-FS is divided into two major parts -- fuse.c and cache.cc(.hh). fuse.c implements the filesystem working as a bridge between local cache and remote storage. cache.cc implements cache operations that are related to reading/writing cache.img and stores the on-disk layout of most recently used data in the cache_dir.

3.2 Fuse Implementation:

RAT-FS filesystem overwrites a bunch of linux system calls using FUSE. Some of them are very simple since there are not a lot of modifications.  Major modification happens on getattr, read, write, create, unlink.

3.2.1 Read & Write: 
-- int rat_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
-- int rat_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)

First thing to do in read/write is to calculate the first_block(which block to start reading/writing), last_block(where block to end reading/writing) and the block offset of both (block_offset in between is always 0). 


rat_read and rat_write implementation:
3.3 Cache Implementation:

In general, it is a LRU cache of a pair of doubly-linked lists working as a free list and a used list. Major functions are cache_fetch() and cache_add().

3.3.1 cache_fetch():
--int cache_fetch(const char* path, uint32_t block_num, uint64_t offset,  char* buf, uint64_t len,ssize_t *bytes_read)



3.3.2 cache_add():
-- int cache_add(const char* path, uint32_t block_num, const char* buf, uint64_t len,ssize_t* bread): 

