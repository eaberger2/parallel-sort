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
int part = 0;

typedef struct{
  int total_records;
  char *mapping;
} f_data;


void merge(int low, int mid, int high){

}

void merge_sort(int low, int high){
  int mid = low + (high - low) / 2;
  if(low < high){
    merge_sort(low,mid);
    merge_sort(mid+1,high);
    merge(low,mid,high);
  }
}

//https://www.geeksforgeeks.org/merge-sort-using-multi-threading/
void* merge_sort(void* arg){
  //split the address space by the the number of threads and have each thread merge sort its own section
  //when threads finish sorting their section have one of the threads merge its section and another, keep going until the sections are all merged -> prob 
  //need to use semaphore for this because the merge sort is producing two sorted sections and the consumer is merging these sections
  int thread_part = part++;
  int low = thread_part * (arg->total_records / THREAD_MAX); //index of first record in thread's section
  int high = (thread_part + 1) * (arg->total_records / THREAD_MAX) - 1; //index of last record in thread's section
  int mid = low + (high - low) + 2;
  if(low < high){ //make sure there isn't only one record in the section
    merge_sort(low, mid);
    merge_sort(mid + 1, high);
    merge(low, mid, high);
  }
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
