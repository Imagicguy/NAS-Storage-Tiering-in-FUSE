#ifndef __BUCKET_H__
#define __BUCKET_H__
char * makebucket(uint64_t number);
char * next_bucket(void);
void bucket_to_head(const char *bucketpath);
#endif
