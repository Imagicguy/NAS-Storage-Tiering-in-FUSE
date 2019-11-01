#include "cache_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>

char* cache_getlink(const char *base, const char* file) {
  char * path = NULL;
  asprintf(path, "%s/%s", base, file);
  char *result = readlink(path);
  if (result == NULL) {
    syslog(LOG_INFO,"cache_getlink: readlink error(result=NULL)\n");
    free(path);
    return NULL;
  }
  free(path);
  return result;
}

char * cache_make_entry(const char *base, const char *dir, uint64_t number){
  char* path = NULL;
  if (dir != NULL) {
    asprintf(&path, "%s/%s/%llu", base, dir, (unsigned long long)number);
  } else {
    asprintf(&path, "%s/%llu", base, (unsigned long long)number);
  }

  if (mkdir(path, 0700) == -1) {
    syslog(LOG_INFO,"cache_make_entry:mkdir error\n");
    return NULL;
  }

  return path;

}
void cache_makelink(const char *base, const char *file, const char *dest){
  char* source = NULL;
  asprintf(&source, "%s/%s", base, file);
  ulink(source);
  if (symlink(dest,source) == -1) {
    syslog(LOG_INFO,"cache_makelink: error in symlink()\n");
  }
  free(source);
}
void cache_insert_to_head(const char *base, const char *path, const char *head,const char *tail){
  char *h = fsll_getlink(base, head);
  char *t = fsll_getlink(base, tail);
  if (h == NULL && t == NULL) {
    cache_makelink(base,head,path);
    cache_makelink(base,tail,path);
    cache_makelink(path,"next",NULL);
    cache_makelink(path,"prev",NULL);
  }else if (h != NULL && t != NULL) {
    cache_makelink(path, "next",h);
    cache_makelink(h,"prev",path);
    cache_makelink(base,head,path);
  }else {
    syslog(LOG_INFO,"cache_insert_to_head: one of head&tail is NULL\n");
  }
  if (h) free(h);
  if (t) free(t);
  
}
void cache_update_to_head(const char *base, const char *path, const char *head, const char *tail){
  char *h = cache_getlink(base, head);
  char *t = cache_getlink(base, tail);
  char *n = cache_getlink(path, "next");
  char *p = cache_getlink(path, "prev");
  syslog(LOG_INFO,"cache_to_head:\n  head: %s\n  tail: %s\n next: %s\n prev: %s\n",h,t,n,p);
  if (h == NULL) {
    syslog(LOG_INFO,"cache_to_head: no head found!\n");

    goto exit;
  }
  if (t == NULL) {
    syslog(LOG_INFO,"in fsll_to_head, no tail found!\n");

    goto exit;
  }

  if (p == NULL) {
    // already head; do nothing
    goto exit;
  } else {
    cache_makelink(p, "next", n);
  }

  if (n) {
    cache_makelink(n, "prev", p);
  } else {
    cache_makelink(base, tail, p);
    // p->next is already NULL from above
  }

  // assuming h != NULL
  fsll_makelink(h, "prev", path);
  fsll_makelink(path, "next", h);
  fsll_makelink(path, "prev", NULL);
  fsll_makelink(base, head, path);

 exit:
  free(h);
  free(n);
  free(p);
  free(t);
}
