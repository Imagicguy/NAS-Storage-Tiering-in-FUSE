#ifndef __CACHE_LINK_H__
#define __CACHE_LINK_H__


char* cache_getlink(const char *base, const char* file);
char * cache_make_entry(const char *base, const char *dir, uint64_t number);
void cache_makelink(const char *base, const char *file, const char *dest);
void cache_insert_to_head(const char *base, const char *path, const char *head,const char *tail);
void cache_update_to_head(const char *base, const char *path, const char *head, const char *tail);
#endif
