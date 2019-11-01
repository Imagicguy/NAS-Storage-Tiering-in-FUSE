#ifndef __CACHE_H__
#define __CACHE_H__

#include<stdlib.h>
#include<errno.h>
void cache_init(const char *cache_dir, uint64_t cache_size, uint64_t bucket_max_size, unsigned long long cache_block_size);
int cache_fetch(const char *filename, uinit32_t block, uint64_t offset, char *buf, uint64_t len, uint64_t *bytes_read);
int cache_add(const char *filename, uint32_t block, const char *buf, uint64_t len);
#endif
