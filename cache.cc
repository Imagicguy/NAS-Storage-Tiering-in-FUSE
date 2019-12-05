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
  next_potato_num = 0;

  return 0;
}

//path format: "a/dog.txt"

//fullpath format: "/home/cachefs/a/dog.txt/2"
int cache_fetch(const char* path, uint32_t block_num, uint64_t offset,  char* buf, uint64_t len,ssize_t *bytes_read) {
  //  fprintf(stderr,"now into cache_fetch()..........\n");

  char fullpath[PATH_MAX];
  snprintf(fullpath, PATH_MAX, "%s%s/%u",cache_dir,path,block_num);


  char data[100];
  FILE* block_ptr = fopen(fullpath, "r");
  if (block_ptr == NULL) {// this block doesn't exist in cache
    return -1;
  }else {
    if (fgets(data, 100, block_ptr) != NULL) {
      puts(data);
    }
  }

  int potato_index = atoi(strtok(data,"#"));

  ssize_t block_offset = atoi(strtok(NULL, "#"));// should be same as offset

  off_t block_read_size = atoi(strtok(NULL, "#"));//should be same as len

  DataNode* node = map[potato_index];
  used_list->refresh(node);

  int fd = open(cache_img,O_RDONLY);
  if (fd == -1) {
    printf("cache_fetch: open cache_img failed %s \n", cache_img);
    return -1;
  }
  *bytes_read = pread(fd, buf, block_read_size, potato_index*potato_size + offset);
  if (*bytes_read  == -1) {
    printf("cache_fetch: read from cache.img failed nread=%ld\n",*bytes_read);
    fclose(block_ptr);
    close(fd);

    return -1;
  }
  fclose(block_ptr);
  close(fd);

  return 0;
}

DataNode* getFreeNode() {

  if (free_list->getSize() > 0 || free_list->getTail()->index() != -1) {    
    return free_list->getTail();
  }
  //no free node avaliable, need to create new one
  if (next_potato_num <  max_potato_num) {// cache not full, create new one
    DataNode* tmp = new DataNode(next_potato_num);

    map[next_potato_num++] = tmp;
    free_list->addToHead(tmp);

    free_list->print();
    

  }else {

    if (used_list->getSize() == 0){
      return NULL;
    }
    DataNode* used_tail  = used_list->getTail();
    if (used_tail == NULL) {
      perror("getFreeNode(): used_tail == NULL");
      return NULL;
    }
    char cache_block_path[PATH_MAX];// /home/cachefs/a/dog.txt/1
    snprintf(cache_block_path,PATH_MAX,"%s%s",cache_dir,used_tail->block_path);
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
    for (auto i = strlen(used_tail->block_path) - 1;i < strlen(used_tail->block_path);i--) {
      if (used_tail->block_path[i] == '/') {
        strncpy(file_related_path,used_tail->block_path,i);
        break;
      }
    }
    

    char file_cloud_path[PATH_MAX];// /cloudfs/a/dog.txt
    snprintf(file_cloud_path, PATH_MAX, "%s%s",cloud_dir,file_related_path);

    if (remove(cache_block_path) != 0) {
      perror("getFreeNode(): Error deleting file");
      close(cache_img_fd);
      return NULL;
    }
    used_list->remove(used_tail);
    free_list->addToHead(used_tail);
    close(cache_img_fd);

  }
  return free_list->getTail();

}
//path: a/dog.txt
int moveToFree(int potato_index){
  DataNode* node = NULL;
  try {
    node = map.at(potato_index);
  }
  catch(const out_of_range &e) {
    cerr << "Exception at " << e.what() << endl;
  }
  if (node == NULL) {
    return -1;
  }

  used_list->remove(node);

  free_list->addToHead(node);

  return 0;
}

bool invalidate(const char* path, uint32_t block_num){
  char cache_path[PATH_MAX];
  snprintf(cache_path,PATH_MAX, "%s%s/%u",cache_dir,path,block_num);

  char data[100];
  FILE* block_ptr = fopen(cache_path, "r");
  if (block_ptr == NULL){
    fprintf(stderr,"failed to open block_ptr for %s\n",cache_path);
    
    return false;
  }else {
    if (fgets(data, 100, block_ptr) != NULL) {
      puts(data);
      int potato_index = atoi(strtok(data, "#"));

      moveToFree(potato_index);
      if (remove(cache_path) != 0) {
	fprintf(stderr,"invalidate(): failed to remove %s\n",cache_path);
      }
    }
  }
  fclose(block_ptr);
  return true;
}

int cache_add(const char* path, uint32_t block_num, const char* buf, uint64_t len,ssize_t* bread){
  fprintf(stderr, "come into cache_add()....\n");
  char file_path[PATH_MAX];
  snprintf(file_path,PATH_MAX,"%s",cache_dir);
  //create dir if needed
  int prev = 0;
  for (auto i = 1;i < strlen(path);i++) {
    if (path[i] == '/') {
      char* path_content = (char*)malloc(i - prev);

      memset(path_content,0,i - prev);
      snprintf(path_content,i - prev + 1,"%s",path + prev);

      snprintf(file_path + strlen(file_path), i - prev + 1, "%s",path_content);

      if (mkdir(file_path, 0700) == -1 && errno != EEXIST) {
	printf("cache_add(): fail to create dir %s\n",file_path);
	return -errno;
      }
      prev = i;
      free(path_content);
    }
  }

  char* path_content = (char*)malloc(strlen(path) - prev);
  memset(path_content,0,strlen(path) - prev);
  snprintf(path_content,strlen(path) - prev + 1,"%s",path + prev);
  snprintf(file_path + strlen(file_path), strlen(path) - prev + 1, "%s",path_content);
  if (mkdir(file_path, 0700) == -1 && errno != EEXIST) {
    printf("cache_add(): fail to create dir %s\n",file_path);
    return -errno;
  }
  free(path_content);
  
  DataNode* empty_node = getFreeNode();


  if (empty_node == NULL) {
    return -1;//can't get empty node, bad!
  }
  
  free_list->remove(empty_node);
  used_list->addToHead(empty_node);
  


  char block_path[PATH_MAX];
  snprintf(block_path, PATH_MAX, "%s/%u",path,block_num);

  strcpy(empty_node->block_path, block_path);
  char cache_block_path[PATH_MAX];
  snprintf(cache_block_path, PATH_MAX, "%s%s", cache_dir, block_path);
  int cache_fd = open(cache_block_path,O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  
  if (cache_fd == -1) {
    perror("cache_add(): cache_fd == -1\n");
    close(cache_fd);
    return - 1;
  }
  char* content = (char*)malloc(128);
  memset(content, 0, 128);
  snprintf(content,128, "%d#%d#%lu\n",empty_node->index(),0,len);
  
  ssize_t bytes_written_content = pwrite(cache_fd,content, 128, 0);
  int cache_img_fd = open(cache_img,O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  int bytes_written_img = pwrite(cache_img_fd,buf,len,empty_node->index() * potato_size + 0);
  *bread = bytes_written_img;
   close(cache_fd);
   close(cache_img_fd);
  
  return 0;

}

