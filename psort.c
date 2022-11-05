#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#define THREAD_MAX get_nprocs()

pthread_mutex_t part_lock;
pthread_mutex_t range_lock;
int part = 0;

typedef struct{
  int total_records;
} f_data;

typedef struct rec{
  int key;
  int value[24];
} record;

record *mapping;

void merge(int low, int mid, int high){
  // 100/4 = 25 so 25 ints per record
  record* left = malloc(sizeof(record) *(mid-low+1));
  record* right = malloc(sizeof(record) * (high-mid));

  int left_size = mid-low+1, right_size = high-mid, i, j;

  for(i = 0; i < left_size; i++){
    left[i] = mapping->key;
    mapping++;
  }
  for(i = 0; i < right_size; i++){
    right[i] = mapping->key;
    mapping++;
  }


  int k = low;
  i = j = 0;
  /*
  while(i < left_size && j < right_size){
    if(left[i] <= right[j]){
      mapping[k++] = left[i++];
    }
    else{
      mapping[k++] = right[j++];
    }
  }*/
}

void merge_sort_more(int low, int high){
  int mid = low + (high - low) / 2;
  if(low < high){
    merge_sort_more(low,mid);
    merge_sort_more(mid+1,high);
    merge(low,mid,high);
  }
}

//https://www.geeksforgeeks.org/merge-sort-using-multi-threading/
void *merge_sort(void* arg){
  //split the address space by the the number of threads and have each thread merge sort its own section
  //when threads finish sorting their section have one of the threads merge its section and another, keep going until the sections are all merged -> prob 
  //need to use semaphore for this because the merge sort is producing two sorted sections and the consumer is merging these sections
  f_data *new_arg = (f_data*)arg;
  int thread_part;
  pthread_mutex_lock(&part_lock);
  thread_part = part;
  part++;
  pthread_mutex_unlock(&part_lock);
  int total_records = 0;
  if(new_arg->total_records % THREAD_MAX != 0){
    total_records = new_arg->total_records + (THREAD_MAX - (new_arg->total_records % THREAD_MAX));
  }
  pthread_mutex_lock(&range_lock);
  int low = thread_part * (total_records / THREAD_MAX); //index of first record in thread's section
  int high = (thread_part + 1) * (total_records / THREAD_MAX) - 1; //index of last record in thread's section
  int mid = low + (high - low) / 2;
  pthread_mutex_unlock(&range_lock);
  if(low < high){ //make sure there isn't only one record in the section
    merge_sort_more(low, mid);
    merge_sort_more(mid + 1, high);
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
  mapping = mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
  if(mapping == MAP_FAILED){
    printf("MAP FAILED\n");
  }

  pthread_mutex_init(&part_lock,NULL);
  pthread_mutex_init(&range_lock,NULL);

  int total_records = size/100; //num of records in file (total bytes/100)
  f_data *file_d;
  file_d = malloc(sizeof(f_data)); //not sure if we need malloc but prob since we are sharing between threads
  file_d->total_records = total_records;
  pthread_t *threads = malloc(sizeof(pthread_t) * THREAD_MAX);
  for(int i=0; i<THREAD_MAX; i++){
    pthread_create(&threads[i], NULL, merge_sort, (void *)file_d); //can only send pointers to the function so send a struct with all the info we need
  }
  pthread_exit(NULL);
  return 0;
}
