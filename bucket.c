#include "bucket.h"
#include "cache_link.h"
char * makebucket(uint64_t number){
  
}

char * next_bucket(void){
  
}

void bucket_to_head(const char *cache_dir, const char *bucketpath){
  cache_update_to_head(cache_dir, bucketpath, "buckets/head", "buckets/tail");
}
