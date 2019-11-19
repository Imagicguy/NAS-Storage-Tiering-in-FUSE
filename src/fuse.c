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
    // fprintf(stderr, "print_getattr(): ready to print file metadata...........\n");
    fprintf(stderr, "stbuf->uid = %d\n", stbuf->st_uid);
    // fprintf(stderr, "stbuf->pid = %d\n", stbuf->st_pid);
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


int rat_getattr(const char *path, struct stat *stbuf) {
    // fprintf(stderr, "rat_getattr(): path =%s\n",path );
    // fprintf(stderr, "rat_getattr(): path len = %d\n",strlen(path));
	fprintf(stderr, "In rat_getattr function-----------------------\n");
    fprintf(stderr, "metadata size = %lu\n",metadata.size());
    // print_metadata_map();
    int retstat = 0;
    char real[64];
    real_path(real, path);

    fprintf(stderr, "real = %s\n",real);
    string real_str(real);
    cout << "real_str = " << real_str << endl;

    ///////////////////////////////////////////////////////////////
	if (metadata.find(real_str) != metadata.end()) {
      // fprintf(stderr, "key: %s\n",metadata.find(real_str)->first);
        perror("rat_getattr(): get metadata from map\n");   
        cout << "the key get from map is : " << metadata.find(real_str)->first << endl;

      ////////
	  *stbuf = metadata.at(real_str);
      fprintf(stderr, "find the file metadata from map,ready to print......................\n");
	  print_getattr(stbuf);
      // print_metadata_map();
      fprintf(stderr, "metadata size = %lu\n",metadata.size());
      fprintf(stderr, "return\n");
	  return 0;
	}	
	///////////////////////////////////////////////////////////////

	retstat = lstat(real, stbuf);
	if (retstat == -1) {  
	  fprintf(stderr,"rat_getattr(): failed to get from s3, try with cache.........\n");
	  return -errno;
	}

    fprintf(stderr, "create new key in metadata: %s\n", real);

    copy_getattr(real, stbuf);
    fprintf(stderr, "create new key in file metadata map, ready to print...........\n");
    print_getattr(stbuf);
    fprintf(stderr, "##############################################################\n");
    // print_metadata_map();
    fprintf(stderr, "metadata size = %lu\n",metadata.size());
    fprintf(stderr, "return\n");  
	return 0;
}

int rat_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	fprintf(stderr, "In rat_read function-----------------------\n");
	// int retval = 0;
    fprintf(stderr, "rat_read(): size=%zu\n",size);
	int bytes_read;
	char* s3_read_buf = NULL;
	fprintf(stderr, "size = %zu\n", size);
	fprintf(stderr, "offset = %jd\n", offset);
	// calculate the first_block number and last_block number
	uint32_t first_block = offset / block_size;
	uint32_t last_block = (offset + size) / block_size;

	fprintf(stderr, "first_block = %d\n", first_block);
	fprintf(stderr, "last_block = %d\n", last_block);
	size_t buf_offset = 0;

	for (uint32_t block = first_block; block <= last_block; block++) {
		off_t block_offset;
		size_t partial_block;
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
		// char real[64];
		// real_path(real, path);
		// struct stat real_stat;
		// if (stat(real, &real_stat) == -1) {
  //           fprintf(stderr, "stat on the s3 file failed\n");
  //           return -errno;
  //       }
        pthread_mutex_lock(&lock);

        fprintf(stderr, "about to call cache_fetch............\n");
        ssize_t bread = 0;


        // 从cache里面找，找到返回true path block 
        // search from cache, return 0 if founded
        fprintf(stderr, "partial block = %zu\n", partial_block);

        int result = cache_fetch(path, block, block_offset, buf + buf_offset, partial_block, &bread);
        // cache里面没有，去s3找，找到加到cache
        // not exist in cache, search from s3 and then added to cache
        if (result == -1) {
            fprintf(stderr, "read from cache failed\n");
        	// cache没找到，去s3找
            // search from s3.....
        	fprintf(stderr, "need to read from s3_dir\n");
        	// fd 从open得到
            // fd got from open()
            int fd = (int)fi->fh;
            s3_read_buf = (char*)malloc(block_size);
            int s3_read = pread(fd, s3_read_buf, block_size, block_size * block);
            if (s3_read == -1) {
            	fprintf(stderr, "read error form s3_dir\n");
                pthread_mutex_unlock(&lock);
            	return -errno;
            }
            else {
            	fprintf(stderr, "read %lu bytes from s3_dir\n", (unsigned long)s3_read);
            	fprintf(stderr, "adding to cache.....about to call cache_add.......\n");
            	
            	ssize_t cache_add_bytes = 0;

            	if (cache_add(path, block, s3_read_buf, block_size, &cache_add_bytes) == -1) {
            		fprintf(stderr, "fail to add block to cache\n");
                    pthread_mutex_unlock(&lock);
                    return -1;
            	}

            	fprintf(stderr, "bytes added to cache = %zd\n", cache_add_bytes);

                memcpy(buf + buf_offset, s3_read_buf + block_offset, ((s3_read < block_size) ? s3_read : block_size));
                free(s3_read_buf);

                if (s3_read < block_size) {
                    // fprintf(stderr, "read less than requested, %lu instead of %lu\n", (unsigned long)s3_read, (unsigned long)block_size);
                    bytes_read += s3_read;
                    fprintf(stderr, "bytes_read after =%lu\n", (unsigned long)bytes_read);       
                } 
                else {
                    fprintf(stderr, "%lu bytes for fuse buffer\n", (unsigned long)block_size);
                    bytes_read += block_size;
                    fprintf(stderr, "bytes_read=%lu\n", (unsigned long)bytes_read);
                }
            }
        }

        else {

        	fprintf(stderr, "file exists in cache_img\n");
        	// bread作为参数传到cache fetch
        	fprintf(stderr, "got %lu bytes from cache\n", (unsigned long)bread);
        	bytes_read += bread;
        	fprintf(stderr, "bytes_read = %lu\n", (unsigned long)bytes_read);
	    }
	    
        pthread_mutex_unlock(&lock);
        buf_offset += partial_block;
	}
    
	return bytes_read;
}

int rat_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

  //////////////////////////////////////////////////////////////////////
    char real[64];
    real_path(real, path);
    string path_str(real);
    if (metadata.find(path_str) != metadata.end()) {
        fprintf(stderr,"rat_write: this file metadata existed in map, changing it size now....\n");
        metadata[path_str].st_size = size;
    }
  ////////////////////////////////////////////////////////////////////


  fprintf(stderr, "In rat_write function-----------------------\n");
  fprintf(stderr, "size == %lu\n",size);
  int bytes_write = 0;
  off_t buf_offset = 0;
  uint32_t first_block = offset / block_size;
  uint32_t last_block = (offset + size) / block_size;
  // bool flag = false;
  // fprintf(stderr, "first_block = %d\n", first_block);
  // fprintf(stderr, "last_block = %d\n", last_block);
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

    pthread_mutex_lock(&lock);

    fprintf(stderr, "writing block %lu, 0x%lx to 0x%lx\n", (unsigned long)block, 
	    (unsigned long)offset + buf_offset,
            (unsigned long)offset + buf_offset + write_size);
    // ssize_t temp_write = pwrite((int)fi->fh, buf + buf_offset, write_size, offset + buf_offset);
    ssize_t cache_add_bytes = 0;

    // write through cache, call pwrtie directly
    fprintf(stderr, "in block %d, write size = %zu\n", block, write_size);

    ssize_t nwritten = pwrite((int)fi->fh, buf + buf_offset, write_size, offset + buf_offset);
    fprintf(stderr, "file descriptor = %lu\n", fi->fh);
    if (nwritten == -1) {
        fprintf(stderr, "pwrite error!!\n");
        pthread_mutex_unlock(&lock);
        return -errno;
    }

    fprintf(stderr,"rat_write(): pwrite(): nwritten= %lu\n", nwritten);
    bytes_write += nwritten;
    pthread_mutex_unlock(&lock);
    /*
    if (cache_add(path, block, buf + buf_offset, write_size, &cache_add_bytes) == -1) {
      fprintf(stderr, "fail to add block to cache\n");
    }
    bytes_write += cache_add_bytes;
    fprintf(stderr, "bytes added to cache = %zd\n", cache_add_bytes);
    buf_offset += write_size;*/
    buf_offset += write_size;
  }
  // close(fi->fh);
  return bytes_write;  
}


int rat_open(const char *path, struct fuse_file_info *fi) {
	fprintf(stderr, "In rat_open function-----------------------\n");
    char real[64];
    real_path(real, path);
    int fd = open(real, O_RDWR | O_CREAT, S_IRWXG);
    if (fd == -1) {
       perror("Open: can not open\n");
    }
    fi->fh = fd;
    // 成功， 已经把fd送到fi->fh
    // close(fd);
    return 0;
}

int rat_opendir(const char *path, struct fuse_file_info *fi) {
    fprintf(stderr, "In rat_opendir function-----------------------\n");
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
	fprintf(stderr, "In rat_readdir function-----------------------\n");
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
    fprintf(stderr, "In rat_releasedir function-----------------------\n");
    if (fi->fh != 0) {
        fprintf(stderr, "releasedir closing directory\n");
        DIR *dir = (DIR*)(intptr_t)(fi->fh);
        fi->fh = 0;
        closedir(dir);
    }
    return 0;
}

int rat_access(const char *path, int mask) {
    fprintf(stderr, "In rat_access function-----------------------\n");
    char real[64];
    real_path(real, path);
    int res = access(real, mask);
    if (res == -1) {
        return -errno;
    }
    return 0;
}


int rat_create(const char *path, mode_t mode, struct fuse_file_info *info) {
    fprintf(stderr, "In rat_create function-----------------------\n");

    char real[PATH_MAX];
    /*
    real_path_cache(real,path);
    snprintf(real + strlen(real),PATH_MAX, "%s","_metadata");
    fprintf(stderr,"rat_create(): real = %s\n",real);
    */
    real_path(real, path);
    int retval = open(real, info->flags | O_CREAT | O_EXCL);
    // if (mkdir(real, 0700) == -1) {
    // 	perror("rat_create(): mkdir() failed");
    // 	return -errno;
    // }
    if (retval == -1) {
    	perror("error on rat_create");
    	return -errno;
    }

    info->fh = retval;
    fprintf(stderr, "file descriptor in create = %d\n", retval);
    int cmod = chmod(real, mode);
    if (cmod == -1) {
        fprintf(stderr, "failure on chmod in rat_create\n");
        return -errno;
    }

    /////////////////////////////////////////////
    struct stat statbuf;
    if (stat(real,&statbuf) != 0) {
      perror("rat_create(): stat(real) != 0\n");
      return -errno;
    }else {
        string path_str(real);
      printf("rat_create(): metadata[%s] storing...\n",path);
      metadata[path_str] = statbuf;
      print_getattr(&statbuf);
    }
    ////////////////////////////////////////////

    return 0;
}

#ifdef HAVE_SYS_XATTR_H

int rat_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
    fprintf(stderr, "In rat_setxattr function-----------------------\n");
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
    fprintf(stderr, "In rat_getxattr function-----------------------\n");
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

int rat_flush(const char *path, struct fuse_file_info *fi) {
    fprintf(stderr, "In rat_flush function-----------------------\n");
    return 0;
}

int rat_utime(const char *path, struct utimbuf *ubuf) {
    fprintf(stderr, "In rat_utime function-----------------------\n");
    return 0;
    char real[64];
    real_path(real, path);
    int ret = utime(real, ubuf);

    /////////////////////////////////////
    string path_str(path);
    if (metadata.find(path_str) != metadata.end()) {
        metadata[path_str].st_mtime = ubuf->modtime;
        metadata[path_str].st_atime = ubuf->actime;
        fprintf(stderr, "st_mtime = %ld\n", ubuf->modtime);
    }
    ////////////////////////////////////

    if (ret == -1) {
        perror("failure on utime");
        return -errno;
    }
    return 0;
}

int rat_truncate(const char *path, off_t newsize) {
  return 0;
  fprintf(stderr, "In rat_truncate function-----------------------\n");
  char real[64];
  real_path(real, path);
  int ret = truncate(real, newsize);
    if (ret == -1) {
      perror("failure on truncate");
      return -errno;
    }
    return 0;
}

int rat_utimens(const char *path, const struct timespec tv[2]) {
    return 0;
    fprintf(stderr, "In rat_utimens function-----------------------\n");
    char real[64];
    real_path(real, path);
    int res = utimensat(0, real, tv, 0);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

int rat_mkdir(const char *path, mode_t mode) {
    fprintf(stderr, "In rat_utimens function-----------------------\n");
    char real[64];
    real_path(real, path);    
    int res = mkdir(real, mode);
    if (res == -1) {
        return -errno;
    }
    return 0;
}


int rat_unlink(const char *path) { 
    fprintf(stderr, "In rat_unlink function-----------------------\n");
    pthread_mutex_lock(&lock);
    char real[64];
    real_path(real, path);
    // remove from s3
    int res = unlink(real);
    if (res == -1) {
        return -errno;
    }
    // remove from cache
    char cache_d[64];
    real_path_cache(cache_d, path);
    DIR *dr = opendir(cache_d);
    struct dirent *de; 
    if (dr == NULL) {
        fprintf(stderr, "Could not open current directory\n" ); 
        return -errno; 
    } 
    // int i = 0;
    while ((de = readdir(dr)) != NULL) {
        fprintf(stderr, "ready to delete %s\n", de->d_name);
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        char file_to_remove[64];
        strcpy(file_to_remove, cache_d);
        strcat(file_to_remove, "/");
        strcat(file_to_remove, de->d_name); 

        char buf[100];
        fprintf(stderr, "file_to_remove = %s\n", file_to_remove);
        FILE * cache_ptr = fopen(file_to_remove, "r");
        if (cache_ptr == NULL) {
            fprintf(stderr, "in rat_unlink, cannot open the file to be deleted\n");
            pthread_mutex_unlock(&lock);
            return -errno;
        }
        if (fgets(buf, 100, cache_ptr) != NULL) {
            puts(buf);
            fclose(cache_ptr);
        }

        int potato_index = atoi(strtok(buf, "#"));
        fprintf(stderr, "potato_index = %d\n", potato_index);

        if (moveToFree(potato_index) == -1) {
            fprintf(stderr, "fail to move potato_index %d to free list!!!!!\n", potato_index);
        }
        remove(file_to_remove);
    }
    remove(cache_d);
    closedir(dr);  
    pthread_mutex_unlock(&lock);   
    return 0; 
}

int rat_rmdir(const char *path) {
    fprintf(stderr, "In rat_unlink function-----------------------\n");
    char real[64];
    real_path(real, path);
    char real_cache[64];
    real_path_cache(real_cache, path);

    // remove from s3 and cache
    if (rmdir(real) == -1 || rmdir(real_cache) == -1) {
        return -errno;
    }
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
    releasedir = rat_releasedir;
    access = rat_access;
    create = rat_create;
    // utime = rat_utime;
    truncate = rat_truncate; 
    flush = rat_flush;   
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


