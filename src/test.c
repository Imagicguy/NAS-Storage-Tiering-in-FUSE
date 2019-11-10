#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(){
  int fd = open("/home/hw210/566ece/project/s3-storage-tiering/src/cache.img",O_RDONLY);
  char* block_buf = (char*)malloc(256);
  int res= pread(fd, block_buf, 256,0);
  if (res == -1) {
    printf("read error%d",1);
  }else {
    printf("%s",block_buf);
  }
  return 0;
}
