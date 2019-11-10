#include "cache.h"
#include <pthread.h>
static char* cache_dir;
static uint64_t cache_size;
static unit64_t bucket_size;

static pthread_mutex_t lock;
int cache_init(const char *cache_dir_input, uint64_t cache_size_input, uint64_t bucket_max_size_input, unsigned long long cache_block_size) {
  syslog(LOG_INFO, "coming to init cache...");
  cache_dir = cache_dir_input;
  cache_size = cache_size_input;
  bucket_max_size = bucket_max_size_input;

  // mkdir bucket/ and map/
  char *buf = NULL;
  asprintf(&buf, "%s/buckets", cache_dir);
  if (mkdir(buf, 0700) == -1 && errno != EEXIST) {
    perror("cache.c: Failed to create /buckets");
    return -1;
  }

  FREE(buf);
  syslog(LOG_INFO, "create bucket/ dir: Done");
  
  asprintf(&buf, "%s/map", cache_dir);
  if (mkdir(buf, 0700) == -1 && errno != EEXIST) {
    perror("cache.c: Failed to create /buckets");
    return -1;
  }
  FREE(buf);
  syslog(LOG_INFO, "create map/ dir: Done");
  
  
  asprintf(&buf, "%s/buckets/bucket_size", cache_dir);
  FILE *f = fopen(buf, "w");
  
  if (f == NULL) {
    perror("cache.c Failed to create buckets/bucket_size");
    return -1;
  }
  fprintf(f,"%llu", bucket_size);
  fclose(f);
  
  free(buf);
  asprintf(&buf, "%s/buckets/head",cache_dir);
  symlink(buf,"dev/null");
  
  
  /*
  unsigned long long old_cache_block_size;
  if (fscanf(f, "%llu", &old_cache_block_size) != 1) {
    perror("cache.c: Failed to get old_cache_block_size");
    go to exit;
  }
  if (old_cache_block_size != cache_block_size) {// no old cache_size specified
    perror("cache.c: old_cache_block_size != cache_block_size");
    go to exit;
  }
  */
  //create bucket/ map/ nextBucketNum, head, tail
  
}


int cache_fetch(const char *filename, uinit32_t block, uint64_t offset, char *buf, uint64_t len, uint64_t *bytes_read){
  syslog(LOG_INFO, "coming to cache_fetch for block %llu of %s...",block,offset);
  pthread_mutex_lock(&lock);
  char mapfile[PATH_MAX];
  snprintf(mapfile, PATH_MAX, "%s/map%s/%lu",cache_dir,filename,(unsigned long) block);
  char bucketpath[PATH_MAX];
  ssize_t readbyte;
  //try to read from cache
  if ((readbyte = readlink(mapfile, bucketpath, PATH_MAX - 1)) == -1){
    if (errno == ENOENT || errno == ENOTDIR) {
      syslog(LOG_INFO,"cache_fetch:block not found in cache\n");
      pthread_mutex_unlock(&lock);
      return -1;
    }else {
      syslog(LOG_INFO,"cache_fetch:unexpected error when readlink()\n");
      pthread_mutex_unlock(&lock);
      return -1;
    }
  }
  // get from cache, move this bucket to head
  bucketpath[readbyte] = '\0';
  move_bucket_to_head(bucketpath);
  //get bucket_data = cache_dir/buckets/xxx/data
  char bucketdata[PATH_MAX];
  snprintf(bucketdata, PATH_MAX, "%s/data", bucketpath);
  uint64_t size = 0;
  struct stat stbuf;
  if (stat(bucketdata,&stbuf) == -1) {
    syslog(LOG_INFO,"cache_fetch:unexpected error when stat()\n");
    pthread_mutex_unlock(&lock);
    return -1;
  }
  size = (unit64_t) stbuf.st_size;
  if (size < offset) {
    syslog(LOG_INFO,"cache_fetch:offset(%llu) > size(%llu) when fetch block\n",offset,size);
    pthread_mutex_unlock(&lock);
    *bytes_read = 0;
    return 0;
  }
  int fd = open(bucketdata,O_RDONLY);
  if (fd == -1) {
    syslog(LOG_INFO,"cache_fetch: failed to open bucketdata dir:%s\n",bucketdata);
    pthread_mutex_unlock(&lock);
    return -1;
  }

  *bytes_read = pread(fd,buf,len,offset);
  if (bytes_read == -1) {
    syslog(LOG_INFO,"cache_fetch: failed to pread(fd)\n");
    close(fd);
    pthread_mutex_unlock(&lock);
    return -1;
  }
  syslog(LOG_INFO,"cache_fetch: expected read:%llu  actual read:%llu\n",(unsigned long long)*bytes_read,(unsigned long long)len);
  close(fd);
  pthread_mutex_unlock(&lock);
  return 0;
}

int cache_add(const char *filename, uint32_t block, const char *buf, uint64_t len){
  //fileandboock = map/file1/3 (this is a symlink!!)
  char blockPath[PATH_MAX];
  snprinf(blockPath, PATH_MAX, "%s/map/%s/%s/%lu",cache_dir,filename,block);
  if (readlink(blockPath) != NULL) {
    syslog(LOG_INFO,"cache_add: block %lu of file %s already existed\n",block,filename);
    return 0;
  }
  
  //
  
  if (symlink != NULL) {
    //  this file/block3 is  already existed. decide by (access(path) ==0)?
    return 0;
  }
  
  
}
