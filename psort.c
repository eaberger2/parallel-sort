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
#include <sys/sysinfo.h>

#define THREAD_MAX get_nprocs()

pthread_mutex_t lock;

typedef struct file_data{
  int total_records;
  char *mapping;
} f_data;

//https://www.geeksforgeeks.org/merge-sort-using-multi-threading/
void* merge_sort(void* arg){

}

int main(int argc, char *argv[]){
  if(argc != 3){
    printf("Argument number error\n");
    exit(1);
  }
  int fd = open(argv[1],O_RDWR);
  if(fd<0){
    printf("File error\n");
  }
  struct stat s;
  int status = fstat(fd,&s);
  if(status < 0){
    printf("Status error\n");
  }
  size_t size = s.st_size;
  char *addr = mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
  if(addr == MAP_FAILED){
    printf("MAP FAILED\n");
  }

  int total_records = size/100; //num of records in file (total bytes/100)
  f_data *file_d;
  file_d = malloc(sizeof(f_data)); //not sure if we need malloc but prob since we are sharing between threads
  file_d->total_records = total_records;
  file_d->mapping = addr;

  pthread_t threads[THREAD_MAX];
  for(int i=0; i<THREAD_MAX; i++){
    pthread_create(&threads[i], NULL, merge_sort, (void*)file_d); //can only send pointers to the function so send a struct with all the info we need
  }

  return 0;
}
