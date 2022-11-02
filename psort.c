#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
  if(argc != 3){
    exit(1);
  }
  int fd = open(argv[1],O_RDONLY||O_WRONLY);
  if(fd<0){
    printf("File error\n");
  }
  struct stat s;
  int status = fstat(fd,&s);
  if(status < 0){
    printf("Status error\n");
  }
  size_t size = s.st_size;
  char *addr = mmap(0,size,PROT_READ||PROT_WRITE,MAP_SHARED||MAP_SYNC,fd,0);
  if(addr == MAP_FAILED){
    printf("MAP FAILED\n");
  }
  printf("Address value please: %c\n",*addr);
  return 0;
}
