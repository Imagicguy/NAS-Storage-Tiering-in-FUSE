#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <pthread.h>
#include <unordered_map>
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
using namespace std;
char rootdir[64];
char cache_dir[64];
char s3_dir[64];
char mount_dir[64];
unsigned long long block_size = 0;
unsigned long long cache_size = 0;
unordered_map <string,struct stat> metadata;
pthread_mutex_t lock;

int cache_fetch(const char* path, uint32_t block_num, uint64_t offset,  char* buf, uint64_t len,ssize_t *bytes_read);

int cache_add(const char* path, uint32_t block_num, const char* buf, uint64_t len,ssize_t* bread);

int cache_init(char *cache_dir_input, uint64_t cache_size_input, unsigned long long cache_block_size, char* cache_img_input, char* cloud_dir_input);

int moveToFree(int potato_index);

int rat_unlink(const char *path);

bool invalidate(const char* path, uint32_t block_num);

void real_path(char* source, const char* relative_path) {

	strcpy(source, s3_dir);
	strcat(source, relative_path);
}

void real_path_root(char* source, const char* relative_path) {
	strcpy(source, rootdir);
	strcat(source, relative_path);
}

void real_path_cache(char* source, const char* relative_path) {
	strcpy(source, cache_dir);
	strcat(source, relative_path);
}

void print_getattr(struct stat *stbuf) {

    fprintf(stderr, "stbuf->uid = %d\n", stbuf->st_uid);

    fprintf(stderr, "stbuf->mode = %d\n", stbuf->st_mode);
    fprintf(stderr, "stbuf->size = %jd\n", stbuf->st_size);
}

void print_metadata_map(){
    for (auto it = metadata.begin(); it != metadata.end();++it) {
        cout << "key: " << it->first << endl;
    }
}

void copy_getattr(const char * path, struct stat * source) {
    struct stat statbuf;
    memset (&statbuf, 0, sizeof(statbuf));
    statbuf.st_size = source->st_size;
    statbuf.st_uid = source->st_uid;
    statbuf.st_gid = source->st_gid;
    statbuf.st_mtime = source->st_mtime;
    statbuf.st_atime = source->st_atime;
    statbuf.st_mode = source->st_mode;
    string a(path);
    metadata.insert({a, statbuf});
    print_getattr(&metadata[a]);
}

int rm_file_from_cache(const char* path){
  // fprintf(stderr, "remove file %s from cache\n",path);
  char cache_d[64];
  real_path_cache(cache_d, path);
  DIR *dr = opendir(cache_d);
  struct dirent *de; 
  if (dr == NULL) {
    fprintf(stderr, "Could not open current directory\n" ); 
    return -errno; 
  } 
  while ((de = readdir(dr)) != NULL) {

    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
      continue;
    }
    char file_to_remove[64];
    strcpy(file_to_remove, cache_d);
    strcat(file_to_remove, "/");
    strcat(file_to_remove, de->d_name); 

    char buf[100];
    FILE * cache_ptr = fopen(file_to_remove, "r");
    if (cache_ptr == NULL) {
      fprintf(stderr, "in rat_unlink, cannot open the file to be deleted\n");

      return -errno;
    }
    if (fgets(buf, 100, cache_ptr) != NULL) {
      puts(buf);
      fclose(cache_ptr);
    }

    int potato_index = atoi(strtok(buf, "#"));

    if (moveToFree(potato_index) == -1) {
      fprintf(stderr, "fail to move potato_index %d to free list!!!!!\n", potato_index);
    }
    remove(file_to_remove);
  }
  remove(cache_d);
  closedir(dr);
  return 0;
}


int rat_link(const char *path, const char *newpath) {

  char fpath[PATH_MAX], fnewpath[PATH_MAX];
  real_path(fpath, path);
  real_path(fnewpath, newpath);
  
  return link(fpath, fnewpath);
}

int rat_chmod(const char *path, mode_t mode) {
    char fpath[PATH_MAX];
    fprintf(stderr,"\nbb_chmod(fpath=\"%s\", mode=0%03o)\n",
            path, mode);
    real_path(fpath, path);
    return chmod(fpath, mode);
}


int rat_getattr(const char *path, struct stat *stbuf) {
    // fprintf(stderr, "In rat_getattr function-----------------------\n");
    int retstat = 0;
    char real[64];
    real_path(real, path);
    /*
      fprintf(stderr, "real = %s\n",real);
      string real_str(real);
      cout << "real_str = " << real_str << endl;
      if (metadata.find(real_str) != metadata.end()) {
      perror("rat_getattr(): get metadata from map\n");   
      cout << "the key get from map is : " << metadata.find(real_str)->first << endl;
      *stbuf = metadata.at(real_str);
      fprintf(stderr, "find the file metadata from map,ready to print......................\n");
      print_getattr(stbuf);
      // print_metadata_map();
      fprintf(stderr, "metadata size = %lu\n",metadata.size());
      fprintf(stderr, "return\n");
      return 0;
      }	  
    */
    retstat = lstat(real, stbuf);
    if (retstat == -1) {  
      fprintf(stderr,"rat_getattr(): failed to get from s3, try with cache.........\n");
      return -errno;
    }
    /*
      fprintf(stderr, "create new key in metadata: %s\n", real);
      copy_getattr(real, stbuf);
      fprintf(stderr, "create new key in file metadata map, ready to print...........\n");
      print_getattr(stbuf);
      fprintf(stderr, "##############################################################\n");
      // print_metadata_map();
      fprintf(stderr, "metadata size = %lu\n",metadata.size());
      fprintf(stderr, "return\n");  
    */
    return 0;
}

int rat_readlink(const char *path, char *link, size_t size) {
  fprintf(stderr,"In rat_readlink function--------------------\n");
  int retstat;
  char fpath[PATH_MAX];
  real_path(fpath, path);
  retstat = readlink(fpath, link, size - 1);
  if (retstat >= 0) {
    link[retstat] = '\0';
    retstat = 0;
  }
  return retstat;
}

int rat_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
  //  fprintf(stderr, "In rat_fsync function---------------------\n");
#ifdef HAVE_FDATASYNC
  if (datasync)
    return fdatasync(fi->fh);
  else
#endif
    return fsync(fi->fh);
}

int rat_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  // fprintf(stderr, "In rat_read function-----------------------\n");
  int bytes_read = 0;
  char* s3_read_buf = NULL;
  // calculate the first_block number and last_block number
  uint32_t first_block = offset / block_size;
  uint32_t last_block = (offset + size) / block_size;
  size_t buf_offset = 0;
  
  for (uint32_t block = first_block; block <= last_block; block++) {
    off_t block_offset = 0;
    size_t partial_block = 0;
    if (block == first_block) {
      block_offset = offset - block * block_size;
    }
    else {
      block_offset = 0;
    }
    if (block == last_block) {
      partial_block = (offset + size) - (block * block_size) - block_offset;
    }
    else {
      partial_block = block_size - block_offset;
    }
    if (partial_block == 0) {
      continue;
    }
    char real[64];
    real_path(real, path);
    struct stat real_stat;
    if (stat(real, &real_stat) == -1) {
      fprintf(stderr, "stat on the s3 file failed\n");
      return -errno;
    }
    //pthread_mutex_lock(&lock);    
    // fprintf(stderr, "about to call cache_fetch............\n");
    ssize_t bread = 0;    
    // search from cache, return 0 if founded
    // fprintf(stderr, "partial block = %zu\n", partial_block);
    int result = cache_fetch(path, block, block_offset, buf + buf_offset, partial_block, &bread);
    // not exist in cache, search from s3 and then added to cache
    if (result == -1) {
      // fprintf(stderr, "read from cache failed\n");
      // search from s3.....
      int fd = (int)fi->fh;
      s3_read_buf = (char*)malloc(block_size);
      int s3_read = pread(fd, s3_read_buf, block_size, block_size * block);
      if (s3_read == -1) {
	fprintf(stderr, "read error form s3_dir\n");
	//pthread_mutex_unlock(&lock);
	return -errno;
      }
      // add most recent used block to cache
      else {
	//fprintf(stderr, "read %lu bytes from s3_dir\n", (unsigned long)s3_read);
	//fprintf(stderr, "adding to cache.....about to call cache_add.......\n");	
	ssize_t cache_add_bytes = 0;
        /*for (int loop = 0; loop < 5; loop++) {
	  if (cache_add(path, block, s3_read_buf + block_offset, block_size, &cache_add_bytes) == 0) {
	    break;
	  }
	  fprintf(stderr, "cache_add retry!!!!!!!!\n");
	}
	*/
	
	if (cache_add(path, block, s3_read_buf + block_offset, block_size, &cache_add_bytes) == -1) {
	  fprintf(stderr, "fail to add block to cache\n");
	}
	
	// fprintf(stderr, "bytes added to cache = %zd\n", cache_add_bytes);
	memcpy(buf + buf_offset, s3_read_buf + block_offset, ((s3_read < block_size) ? s3_read : block_size));
	free(s3_read_buf);
	s3_read_buf = NULL;
	// must read the last block
	if (s3_read < block_size) {
	  bytes_read += s3_read;
	  return bytes_read;
	} 
	else {
	  bytes_read += block_size;
	}
	  //bytes_read += s3_read;
      }
    }
    // cache fetch succeed, read from cache
    else {
      // fprintf(stderr, "file exists in cache_img\n");
      bytes_read += bread;
      // fprintf(stderr, "bytes_read = %lu\n", (unsigned long)bytes_read);
    }
    //pthread_mutex_unlock(&lock);
    buf_offset += partial_block;
  }	  
  return bytes_read;
}

int rat_truncate(const char *path, off_t newsize) {
  //  fprintf(stderr, "In rat_truncate function-----------------------\n");
  char real[64];
  real_path(real, path);
  fprintf(stderr,"real = %s\n",real);
  int ret = truncate(real, newsize);
    if (ret == -1) {
      perror("failure on truncate");
      return -errno;
    }
  return 0;
}
int rat_rename(const char *path, const char *newpath) {
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    real_path(fpath, path);
    real_path(fnewpath, newpath);
    int res = rename(fpath, fnewpath);
    return res;
}

int rat_symlink(const char *path, const char *link) {
    char flink[PATH_MAX];
    real_path(flink, link);
    int res = symlink(path, flink);
    return res;
}

int rat_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  // check if in cache, if so, invalidate those blocks in cache for updating...
  if (rm_file_from_cache(path) == -1) {
    fprintf(stderr,"this file not in cache\n");
  }
  int bytes_write = 0;
  off_t buf_offset = 0;
  uint32_t first_block = offset / block_size;
  uint32_t last_block = (offset + size) / block_size;
  
  for (uint32_t block = first_block; block <= last_block; block++) {
    // in each block, number of bytes that can be written, like partial_block in rat_read()
    size_t write_size;
    if (block == first_block) {
        write_size = ((block + 1) * block_size) - offset;
    }
    else if (block == last_block){
      write_size = size - buf_offset;
    }

    else {
      write_size = block_size;
    }

    if (write_size > size) {
      write_size = size;
    }

    if (write_size == 0) {
      continue;
    }
    // pthread_mutex_lock(&lock);
    // fprintf(stderr, "writing block %lu, 0x%lx to 0x%lx\n", (unsigned long)block, 
    //	    (unsigned long)offset + buf_offset,
    //      (unsigned long)offset + buf_offset + write_size);
    // ssize_t temp_write = pwrite((int)fi->fh, buf + buf_offset, write_size, offset + buf_offset);
    ssize_t cache_add_bytes = 0;
    // write through cache, call pwrite directly
    ssize_t nwritten = pwrite((int)fi->fh, buf + buf_offset, write_size, offset + buf_offset);
    //    fprintf(stderr, "file descriptor = %lu\n", fi->fh);
    if (nwritten == -1) {
        fprintf(stderr, "pwrite error!!\n");
        //pthread_mutex_unlock(&lock);
        return -errno;
    }
    // fprintf(stderr,"rat_write(): pwrite(): nwritten= %lu\n", nwritten);
    bytes_write += nwritten;
    //pthread_mutex_unlock(&lock);
    buf_offset += write_size;
  }
   return bytes_write;  
}


int rat_open(const char *path, struct fuse_file_info *fi) {
  //  fprintf(stderr, "In rat_open function-----------------------\n");
  char real[64];
  real_path(real, path);
  //  fprintf(stderr,"rat_open real %s\n",real);
  int fd = open(real,fi->flags);
  if (fd == -1) {
    perror("Open: can not open\n");
  }
  fi->fh = fd;
  return 0;
}

int rat_opendir(const char *path, struct fuse_file_info *fi) {
  // fprintf(stderr, "In rat_opendir function-----------------------\n");
    char real[64];
    real_path(real, path);
    DIR *dir = opendir(real);
    if (dir == NULL) {
        fprintf(stderr, "failed to open dir\n");
        return -1;
    }
    fi->fh = (uint64_t)(intptr_t)dir;
    return 0;
}

int rat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
  // fprintf(stderr, "In rat_readdir function-----------------------\n");
    DIR *dir = (DIR*)(intptr_t)(fi->fh);
    struct dirent *entry = NULL;
    if (dir == NULL) {
        fprintf(stderr, "got null dir handle\n");
        return -1;
    }
    char real[64];
    real_path(real, path);

    while ((entry = readdir(dir)) != NULL) {
        filler(buf, entry->d_name, NULL, 0);
    }
    return 0;
}

int rat_releasedir(const char *path, struct fuse_file_info *fi) {
  //    fprintf(stderr, "In rat_releasedir function-----------------------\n");
    if (fi->fh != 0) {
        fprintf(stderr, "releasedir closing directory\n");
        DIR *dir = (DIR*)(intptr_t)(fi->fh);
        fi->fh = 0;
        closedir(dir);
    }
    return 0;
}

int rat_access(const char *path, int mask) {
  //fprintf(stderr, "In rat_access function-----------------------\n");
    char real[64];
    real_path(real, path);
    int res = access(real, mask);
    if (res == -1) {
        return -errno;
    }
    return 0;
}


int rat_create(const char *path, mode_t mode, struct fuse_file_info *info) {
  //fprintf(stderr, "In rat_create function-----------------------\n");

    char real[PATH_MAX];
    /*
      real_path_cache(real,path);
      snprintf(real + strlen(real),PATH_MAX, "%s","_metadata");
      fprintf(stderr,"rat_create(): real = %s\n",real);
    */
    real_path(real, path);
    int retval = 0;
    retval = open(real, info->flags | O_CREAT | O_EXCL);
    if (retval == -1) {
    	perror("error on rat_create");
    	return -errno;
    }

    info->fh = retval;
    //    fprintf(stderr, "file descriptor in create = %d\n", retval);
    int cmod = chmod(real, mode);
    if (cmod == -1) {
        fprintf(stderr, "failure on chmod in rat_create\n");
        return -errno;
    }
    /*
      struct stat statbuf;
      if (stat(real,&statbuf) != 0) {
      fprintf(stderr, "rat_create(): stat(real) != 0\n");
      return -errno;
      }else {
      string path_str(real);
      printf("rat_create(): metadata[%s] storing...\n",path);
      //      statbuf.st_mode = 010064;
      metadata[path_str] = statbuf;
      
      print_getattr(&statbuf);
      }
    */
    return 0;
}

#ifdef HAVE_SYS_XATTR_H

int rat_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
  //    fprintf(stderr, "In rat_setxattr function-----------------------\n");
    char real[64];
    real_path(real, path);
    int ret = lsetxattr(real, name, value, size, flags);
    if (ret == -1) {
        perror("failure on setxattr");
        return -errno;
    }
    return 0;
}

int rat_getxattr(const char *path, const char *name, char *value, size_t size) {
  // fprintf(stderr, "In rat_getxattr function-----------------------\n");
    char real[64];
    real_path(real, path);
    int ret = getxattr(real, name, value, size);
    if (ret == -1) {
        perror("failure on getxattr");
        return -errno;
    }
    return 0;
}

#endif


int rat_utime(const char *path, struct utimbuf *ubuf) {
  fprintf(stderr, "In rat_utime function-----------------------\n");
  char real[64];
  real_path(real, path);
  int ret = utime(real, ubuf);
  
  if (ret == -1) {
    perror("failure on utime");
    return -errno;
  }
  return 0;
}

int rat_mkdir(const char *path, mode_t mode) {

    char real[64];
    real_path(real, path);    
    int res = mkdir(real, mode);
    if (res == -1) {
        return -errno;
    }
    return 0;
}


int rat_unlink(const char *path) { 
  //   fprintf(stderr, "In rat_unlink function-----------------------\n");
    //pthread_mutex_lock(&lock);
    char real[64];
    real_path(real, path);
    // remove from s3
    fprintf(stderr,"rat_unlink %s\n",real);
    int res = unlink(real);
    if (res == -1) {
      //pthread_mutex_unlock(&lock);
        return -errno;
    }
    /*
    // remove from file metadata
    string real_str(real);
    cout << "real_str = " << real_str << endl;
    if (metadata.find(real_str) != metadata.end()) {
    perror("rat_unlink(): get metadata from map\n");
    metadata.erase(real_str);
    }
    
    */
    // remove from cache
    //pthread_mutex_unlock(&lock);
    if (rm_file_from_cache(path) != 0) {
      fprintf(stderr, "cannot remove file from cache\n");
    }
    
    return 0; 
}

int rat_rmdir(const char *path) {
  //    fprintf(stderr, "In rat_rmdir function-----------------------\n");
    char real[64];
    real_path(real, path);
    char real_cache[64];
    real_path_cache(real_cache, path);

    // remove from s3 and cache
    if (remove(real) == -1) {
      fprintf(stderr, "cannot remove dir from remote_dir: %d\n", -errno);
    }

    if(remove(real_cache) == -1) {
      fprintf(stderr, "cannot remove dir from cache_dir\n");
    }
    
    return 0;
 }

int rat_release(const char *path, struct fuse_file_info *info) {
  
    if (info->fh != 0) {
        // If we saved a file handle here from 
        close((int)info->fh);
    }

    // FUSE ignores the return value here.
    return 0;
}

static struct rat_operations : fuse_operations {
  rat_operations() {
    getattr = rat_getattr;
    read = rat_read;
    write = rat_write;
    open = rat_open;
    opendir = rat_opendir;
    readdir = rat_readdir;
    rename = rat_rename;
    readlink = rat_readlink;
    symlink = rat_symlink;
    releasedir = rat_releasedir;
    release = rat_release;
    //    fsync = rat_fsync;
    //    mknod = rat_mknod;
    chmod = rat_chmod;
    access = rat_access;
    create = rat_create;
    utime = rat_utime;
    truncate = rat_truncate; 
    //    flush = rat_flush;   
    mkdir = rat_mkdir;
    unlink = rat_unlink;
    rmdir = rat_rmdir;
    // utimens = rat_utimens;
#ifdef HAVE_SYS_XATTR_H
    getxattr = rat_getxattr;
    setxattr = rat_setxattr;
#endif
  }
} rat_oper_init;


int main(int argc, char **argv) {
	// get current working directory: 
	struct rat_operations oper();

	getcwd(rootdir, sizeof(rootdir));
	strcat(rootdir, "/");

	char cache_img_input[64];
	const char *temp = "cache.img";
	real_path_root(cache_img_input, temp);
	real_path_root(cache_dir, argv[argc - 5]);
	real_path_root(s3_dir, argv[argc - 4]);
	real_path_root(mount_dir, argv[argc - 3]);

	argv[argc - 5] = cache_dir;
	argv[argc - 4] = s3_dir;
	argv[argc - 3] = mount_dir;

	//	char * cache_image = NULL;
	char *str_block_size = argv[argc - 1];
	char *endptr_block_size;
	block_size = strtoull(str_block_size, &endptr_block_size, 10);
	// fprintf(stderr, "convert char to unsigned long int = %llu\n", block_size);
	char *str_cache_size = argv[argc - 2];
	char *endptr_cache_size;
	cache_size = strtoull(str_cache_size, &endptr_cache_size, 10);

	fprintf(stderr, "cache_input_img = %s\n", cache_img_input);
	fprintf(stderr, "cache_dir = %s\n", argv[argc - 5]);
	fprintf(stderr, "s3_dir = %s\n", argv[argc - 4]);
	fprintf(stderr, "mount_dir = %s\n", argv[argc - 3]);
	fprintf(stderr, "cache_size = %s\n", argv[argc - 2]);
	fprintf(stderr, "block_size = %s\n", argv[argc - 1]);

	argv[argc - 5] = argv[argc - 3];
	argv[argc - 4] = NULL;
	argv[argc - 3] = NULL;
	argv[argc - 2] = NULL;
	argv[argc - 1] = NULL;
	argc -= 4;
	if (cache_init(cache_dir, cache_size, block_size, cache_img_input, s3_dir) == 0) {
		fprintf(stderr, "cache init success......\n");
	}
	// init mutex lock
	pthread_mutex_init(&lock, NULL);
	// return fuse_main(argc, argv, &rat_operations, NULL);
	return fuse_main(argc, argv, &rat_oper_init, NULL);
}



