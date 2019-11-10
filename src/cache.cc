#include "cache.hh"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char* cache_dir;
static uint64_t cache_size;
static unit64_t potato_size;
static unit64_t max_potato_num;
static pthread_mutex_t lock;
static char* cache_img;
unsigned long next_potato_num;
DLList* free_list;
DLList* used_list;
unordered_map<uint32_t index, DataNode* node> map;

int cache_init(const char *cache_dir_input, uint64_t cache_size_input, uint64_t bucket_max_size_input, unsigned long long cache_block_size) {
  fprintf(stderr, "comes into cache_init...\n");
  
  cache_dir = cache_dir_input;
  cache_size = cache_size_input;
  bucket_max_size = bucket_max_size_input;
  max_potato_num = cache_size / potato_size;
  free_list = new DLList();
  used_list = new DLList();
  
  next_potato_num = 0;
}

//path format: "/home/cachefs/a"

//fullpath format: "/home/cachefs/a/dog.txt/2"
int cache_fetch(const char* path, uint32_t block_num, uint64_t offset,  char* buf, uint64_t len,uint64_t *bytes_read) {
  
  char fullpath[PATH_MAX];
  snprintf(fullpath, PATH_MAX, "%s/%lu",path,block_num);
  
  pthread_mutex_lock(&lock);
  FILE* block_ptr = fopen(path, "r");
  if (block_ptr == NULL) {// this block doesn't exist in cache
    return -1;
  }
  int potato_index = atoi(strtok(fullpath,"|"));
  
  //char* block_offset = strtok(NULL, "|");
  //char* block_read_size = strtok(NULL, "|");
  DataNode* node = map[block_index];
  free_list->refresh(node);
  
  int fd = open(cache_img,O_RDONLY);
  int nread = pread(fd, buf, potato_index* potato_size + offset, len);
  
  if (nread == -1) {
    printf("cache_fetch: read from cache.img failed nread=%d\n",nread);
    fclose(block_ptr);
    close(fd);
    return -1;
  }
  
  fclose(block_ptr);
  close(fd);
  return nread;
}

int cache_add(){
  
}

void newNode() {
  
  DataNode* tmp = new DataNode(next_potato_num);
  map[next_potato_num++] = tmp;
  free_list->addToTail(tmp);
  
}
