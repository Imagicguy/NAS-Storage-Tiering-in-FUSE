#include "bucket.h"
#include "cache_link.h"
char * makebucket(uint64_t number){
  
}

char * next_bucket(void){
  /*
  if (!has_free_bucket) {
    delete used_tail
    free the file the used_tail belonged to
    add all related bucket to free_head
  }
     write data to free_tail
    move this bucket to used_head
    delete this bucket from free_tail

}

*/
}

void bucket_to_head(const char *cache_dir, const char *bucketpath){
  cache_update_to_head(cache_dir, bucketpath, "buckets/head", "buckets/tail");
}
