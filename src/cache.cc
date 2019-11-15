#include "cache.hh"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include <unordered_map>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;
static char* cache_dir;
static uint64_t cache_size;
static uint64_t potato_size;
static uint64_t max_potato_num;
static pthread_mutex_t lock;
static char* cache_img;
static char* cloud_dir;
unsigned long next_potato_num;
DLList* free_list;
DLList* used_list;
unordered_map<int, DataNode*> map;

int cache_init(char *cache_dir_input, uint64_t cache_size_input, unsigned long long cache_block_size, char* cache_img_input, char* cloud_dir_input) {
  fprintf(stderr, "comes into cache_init...\n");
  cloud_dir = cloud_dir_input;
  cache_dir = cache_dir_input;
  cache_size = cache_size_input;
  potato_size = cache_block_size;
  max_potato_num = cache_size / potato_size;
  cache_img = cache_img_input; 
  free_list = new DLList();
  used_list = new DLList();
  /*  
  DataNode* node0 = new DataNode(0);
  node0->block_path = (char*)"dog.txt/0";
  used_list->addToHead(node0);
  map[0]= node0;
  
  DataNode* node1 = new DataNode(1);
  node1->block_path = (char*)"dog.txt/1";
  map[1] = node1;
  used_list->addToHead(node1);
  
  DataNode* node2 = new DataNode(2);
  node2->block_path = (char*)"dog.txt/2";
  used_list->addToHead(node2);
  map[2]= node2;
  DataNode* node3 = new DataNode(3);
  node3->block_path = (char*)"dog.txt/3";
  free_list->addToHead(node3);
  */
  next_potato_num = 0;
  //  used_list->print();
  return 0;
  }

//path format: "a/dog.txt"

//fullpath format: "/home/cachefs/a/dog.txt/2"
int cache_fetch(const char* path, uint32_t block_num, uint64_t offset,  char* buf, uint64_t len,ssize_t *bytes_read) {
  char fullpath[PATH_MAX];
  snprintf(fullpath, PATH_MAX, "%s%s/%u",cache_dir,path,block_num);
  //  pthread_mutex_lock(&lock);

  char data[100];
  FILE* block_ptr = fopen(fullpath, "r");
  if (block_ptr == NULL) {// this block doesn't exist in cache
    //    pthread_mutex_unlock(&lock);
    return -1;
  }else {
    if (fgets(data, 100, block_ptr) != NULL) {
      puts(data);
      fclose(block_ptr);
    }
  }
  
  
  printf("cache_fetch(): full_path: %s\n", fullpath);

  int potato_index = atoi(strtok(data,"#"));
  printf("potato_index: %d\n",potato_index);
  char* block_offset = strtok(NULL, "#");// should be same as offset
  printf("block_offset: %s\n",block_offset);
  char* block_read_size = strtok(NULL, "#");//should be same as len
  printf("block_read_size: %s\n", block_read_size);
  DataNode* node = map[potato_index];
  used_list->refresh(node);
  used_list->print();
  int fd = open(cache_img,O_RDONLY);
  if (fd == -1) {
    printf("cache_fetch: open cache_img failed %s \n", cache_img);
  }
  *bytes_read = pread(fd, buf, len, potato_index* potato_size + offset);
  
  if (*bytes_read  == -1) {
    printf("cache_fetch: read from cache.img failed nread=%ld\n",*bytes_read);
    fclose(block_ptr);
    close(fd);
    //    pthread_mutex_unlock(&lock);
    return -1;
  }
  
  fclose(block_ptr);
  close(fd);
  //  pthread_mutex_unlock(&lock);
  return 0;
}

DataNode* getFreeNode() {
  if (free_list->getSize() != 0) {
    printf("have free node %d\n",free_list->getSize());
    return free_list->getTail();
  }
  //no free node avaliable, need to create new one
  if (next_potato_num <  max_potato_num) {// cache not full, create new one
    DataNode* tmp = new DataNode(next_potato_num);
    map[next_potato_num++] = tmp;
    free_list->addToHead(tmp);
    printf("create new one free_node:%lu\n", next_potato_num);
  }else {
    DataNode* used_head  = used_list->getHead();
    if (used_head == NULL) {
      perror("getFreeNode(): used_head == NULL");
      return NULL;
    }
    char cache_block_path[PATH_MAX];// /home/cachefs/a/dog.txt/1
    snprintf(cache_block_path,PATH_MAX,"%s/%s",cache_dir,used_head->block_path);
    char data[100];

    FILE* cache_ptr = fopen(cache_block_path,"r");
    if (cache_ptr == NULL) {
      perror("getFreeNode(): failed with cache_ptr\n");
      return NULL;
    }else {
      if (fgets(data,100,cache_ptr) != NULL) {
	puts(data);
	fclose(cache_ptr);
      }
    }
        
    int potato_index = atoi(strtok(data,"#"));
    int potato_offset = atoi(strtok(NULL,"#"));
    int potato_read_len = atoi(strtok(NULL,"#"));
    char* buf = (char*)malloc(potato_size);
    int cache_img_fd = open(cache_img, O_RDONLY);
    if (cache_img_fd == -1) {
      perror("getFreeNode(): failed with cache_img_fd\n");
    }
    
    int bytes_read = pread(cache_img_fd, buf,potato_read_len,potato_index* potato_size + potato_offset);
    if (bytes_read != potato_read_len) {
      perror("Ewwww, bytes_read != potato_read_len\n");
      return NULL;
    }

    char file_related_path[PATH_MAX];// a/dog.txt
    for (auto i = strlen(used_head->block_path) - 1;i < strlen(used_head->block_path);i--) {
      if (used_head->block_path[i] == '/') {
	strncpy(file_related_path,used_head->block_path,i);
	break;
      }
    }
    
    printf("file_related_path: %s\n", file_related_path);
    char file_cloud_path[PATH_MAX];// /cloudfs/a/dog.txt
    snprintf(file_cloud_path, PATH_MAX, "%s/%s",cloud_dir,file_related_path);
    printf("file_cloud_path: %s\n",file_cloud_path);
    int cloud_fd = open(file_cloud_path,O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (cloud_fd == -1) {
      perror("getFreeNode(): failed to open write back file in cloud\n");
      return NULL;
    }
    int bytes_written = pwrite(cloud_fd,buf,potato_read_len,potato_index*potato_size + potato_offset);
    if (bytes_written != potato_read_len) {
      perror("Ewwww, bytes_written != potato_read_len\n");
      close(cloud_fd);
      return NULL;
    }

    if (remove(cache_block_path) != 0) {
      perror("getFreeNode(): Error deleting file");
      return NULL;
    }
    used_list->remove(used_head);
    free_list->addToHead(used_head);
    close(cache_img_fd);
    close(cloud_fd);
  }
  return free_list->getTail();
    
}
//path: a/dog.txt
int cache_add(const char* path, uint32_t block_num, const char* buf, uint64_t len,ssize_t* bread){
  char file_path[PATH_MAX];
  snprintf(file_path,PATH_MAX,"%s",cache_dir);
    //create dir if needed
  int prev = 0;
  for (auto i = 1;i < strlen(path);i++) {
    if (path[i] == '/') {
      snprintf(file_path, PATH_MAX, "%s%s",file_path,path.substr(prev,i - prev));
      if (mkdir(file_path, 0700) == -1 && errno != EEXIST) {
	printf("cache_add(): fail to create dir %s\n",file_path);
	return -errno;
      }
      
      prev = i;
    }
  }
  snprintf(file_path, PATH_MAX, "%s%s",file_path,path.substr(prev));
  if (mkdir(file_path, 0700) == -1 && errno != EEXIST) {
    printf("cache_add(): fail to create dir %s\n",file_path);
    return -errno;
  }

  
  DataNode* empty_node = getFreeNode();
  if (empty_node == NULL) {
    return -1;//can't get empty node, bad!
  }
  free_list->remove(empty_node);
  used_list->addToHead(empty_node);
  
  char block_path[PATH_MAX];
  snprintf(block_path, PATH_MAX, "%s/%u",path,block_num);
  empty_node->block_path = block_path;
  char cache_block_path[PATH_MAX];
  snprintf(cache_block_path, PATH_MAX, "%s/%s", cache_dir, block_path);
  int cache_fd = open(cache_block_path,O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

  if (cache_fd == -1) {
    perror("cache_add(): cache_fd == -1\n");
    return - 1;
  }
  char* content = (char*)malloc(128);
  memset(content, 0, 128);
  snprintf(content,128, "%d#%d#%lu\n",empty_node->index(),0,len);
  
  ssize_t bytes_written_content = pwrite(cache_fd,content, 128, 0);
  int cache_img_fd = open(cache_img,O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  int bytes_written_img = pwrite(cache_img_fd,buf,len,block_num*potato_size + 0);
  
  if (bytes_written_img <= len) {
    while (bytes_written_img != len) {
      ssize_t more_bytes_written = write(cache_img_fd, buf + bytes_written_img, len - bytes_written_img);
      bytes_written_img += more_bytes_written;
    }
  }
  *bread = bytes_written_content;
  close(cache_fd);
  close(cache_img_fd);
  return 0;
  //real read & mkdir for new file if necessary
  //put into free
}


/*
int main() {
  char* cache_dir_input = (char*)"/home/hw210/566ece/project/s3-storage-tiering/src/cachefs";
  char* cache_img = (char*)"/home/hw210/566ece/project/s3-storage-tiering/src/cache.img";
  uint64_t cache_size_input = 16;
  unsigned long long cache_block_size = 4;
  char* cloud_dir = (char*)"/home/hw210/566ece/project/s3-storage-tiering/src/cloudfs";
  cache_init(cache_dir_input, cache_size_input, cache_block_size, cache_img,cloud_dir);

  //********************************
  // cache_fetch() test
  printf("cache_fetch()**********************%d\n",1);
  char* block_buf = (char*)malloc(cache_block_size);
  ssize_t bread = 0;
  char* file_path = (char*)"dog.txt";
  int res = cache_fetch(file_path,0,0,block_buf, cache_block_size,&bread);
  if (res == -1) {
    printf("cache_fetch() test cache miss: %d", res);
  }
  printf("get from buf: %s\n", block_buf);
  printf("byte read: %ld\n", bread);
  //********************************
  
  printf("getFreeNode()********************%d\n",2);
  getFreeNode();
  getFreeNode();

  printf("cache_add()********************%d\n",3);
  char* write_buf = (char*)"qwer";
  cache_add(file_path,2,write_buf,4,&bread);
}
*/
