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
#include <semaphore.h> //need to compile code with -lpthread -lrt so maybe we do need a makefile

#define THREAD_MAX get_nprocs()

pthread_mutex_t part_lock;
pthread_mutex_t range_lock;
pthread_mutex_t merge_permission;
int part = 0;

typedef struct{
  int total_records;
  sem_t *semaphores;
} f_data;

typedef struct rec{
  int key;
  char value[97];
} record;

record *mapping;

void merge(int low, int mid, int high){
  // 100/4 = 25 so 25 ints per record
  record left[mid-low+1];
  record right[high-mid];

  int left_size = mid-low+1, right_size = high-mid, i, j, k;

  for(i = 0; i < left_size; i++){
    left[i] = mapping[low+i];
  }
  for(j = 0; j < right_size; j++){
    right[j] = mapping[mid+1+j];
  }

  k = low;
  i = j = 0;
  while(i < left_size && j < right_size){
    if(left[i].key <= right[j].key){
      mapping[k++] = left[i++];
    }
    else{
      mapping[k++] = right[j++];
    }
  }

  while(i < left_size){
    mapping[k++] = left[i++];
  }

  while(j < right_size){
    mapping[k++] = right[j++];
  }
  return;
}

void merge_sort_more(int low, int high){
  if(low < high){
    int mid = low + (high - low) / 2;
    merge_sort_more(low,mid);
    merge_sort_more(mid+1,high);
    merge(low,mid,high);
  }
  return;
}

void *merge_sort(void* arg){

  f_data *new_arg = (f_data*)arg;
  int thread_part;
  pthread_mutex_lock(&part_lock);
  thread_part = part;
  part++;
  pthread_mutex_unlock(&part_lock);
  int total_records = 0;
  //round up total records
  sem_t * semaphores = new_arg->semaphores;
  int rounded = 0;
  if(new_arg->total_records % THREAD_MAX != 0){
    total_records = new_arg->total_records + (THREAD_MAX - (new_arg->total_records % THREAD_MAX));
    rounded = 1;
  }
  else{
    total_records = new_arg->total_records;
  }
  pthread_mutex_lock(&range_lock);
  int low = thread_part * (total_records / THREAD_MAX); //index of first record in thread's section
  int high;
  if(thread_part == (THREAD_MAX - 1) && rounded){
    high = (thread_part + 1) * (total_records / THREAD_MAX) - (1 + (THREAD_MAX - (new_arg->total_records % THREAD_MAX)));
  }
  else{
    high = (thread_part + 1) * (total_records / THREAD_MAX) - 1; //index of last record in thread's section
  }
  int mid = low + (high - low) / 2;
  pthread_mutex_unlock(&range_lock);

  if(low < high){ //make sure there isn't only one record in the section
    merge_sort_more(low, mid);
    merge_sort_more(mid + 1, high);
    merge(low, mid, high);
  }

  printf("semaphore index: %d\n",thread_part/2);
  sem_post(&semaphores[thread_part/2]); //post when done merging
  printf("finished merging: %d\n",thread_part);
  if(thread_part % 2 != 0){
    printf("get rid of odd threads: %d\n",thread_part);
    return (void *)0; //gets rid of odd threads
  }
  else{
    sem_wait(&semaphores[thread_part/2]); //if even thread wait for odd thread to post
  }

  if(thread_part % 2 == 0){
    printf("Sections being merged: %d and %d\n",thread_part,thread_part+1);
    merge(thread_part * (total_records / THREAD_MAX), (thread_part + 1) * (total_records / THREAD_MAX) - 1, (thread_part + 2) * (total_records / THREAD_MAX) -1);
  }

  return (void *)0;
}

int main(int argc, char *argv[]){
  if(argc != 3){
    printf("Argument number error\n");
    exit(1);
  }
  int fd = open(argv[1],O_RDWR);
  if(fd<0){
    printf("File error\n");
    exit(1);
  }
  struct stat s;
  int status = fstat(fd,&s);
  if(status < 0){
    printf("Status error\n");
    exit(1);
  }
  size_t size = s.st_size;
  char *address;
  address = (char *)mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
  if(address == MAP_FAILED){
    printf("MAP FAILED\n");
    exit(1);
  }
  close(fd);

  int total_records = size/100; //num of records in file (total bytes/100)

  mapping = (record *)malloc(total_records*sizeof(record));
  record *curr = mapping;

  //starts at first address in mapping and increments by size of record
  for(char *val = address; val < address + size; val+=100){
    curr->key = *(int *)val; //gets first int at address val
    //printf("Key: %d\n",curr->key);
    memcpy(curr->value,val+sizeof(int),96);
    curr++;
  }

  pthread_mutex_init(&part_lock,NULL);
  pthread_mutex_init(&range_lock,NULL);
  
  sem_t *semaphores = malloc(sizeof(sem_t) * (THREAD_MAX/2));
  //initialize semaphores
  for(int i=0; i<(THREAD_MAX/2); i++){
    sem_init(&semaphores[i],0,0); //initialize to -2 because threads that finish sorting will increment by 1 until there are two sections to be sorted
  }

  f_data *file_d;
  file_d = malloc(sizeof(f_data)); //not sure if we need malloc but prob since we are sharing between threads
  file_d->total_records = total_records;
  file_d->semaphores = semaphores; //send called function list of semaphores
  pthread_t *threads = malloc(sizeof(pthread_t) * THREAD_MAX);
  int i;
  for(i=0; i<THREAD_MAX; i++){
    pthread_create(&threads[i], NULL, merge_sort, (void *)file_d); //can only send pointers to the function so send a struct with all the info we need
    printf("thread id? %ld\n",threads[i]);
  }

  pthread_join(threads[--i],NULL);

  int count = 0;
  while(count < 36){
    if(count%6 == 0){
      printf("part: %d\n",count/6);
    }
    printf("key: %d\n",mapping[count].key);
    count++;
  }

  pthread_exit(NULL);
  free(mapping);
  free(threads);
  return 0;
}
