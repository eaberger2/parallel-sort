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
#include <errno.h>
#include <semaphore.h> //need to compile code with -lpthread -lrt so maybe we do need a makefile

#define THREAD_MAX get_nprocs()

extern int errno;

void merge_sections(int numb, int sect_size, int total_records);

pthread_mutex_t part_lock;

int part = 0;
int cpus; 
typedef struct{
  int total_records;
} f_data;

typedef struct rec{
  int key;
  char value[96];
} record;

record *mapping;

void merge(int low, int mid, int high){
  // 100/4 = 25 so 25 ints per record
  //record left[mid-low+1];
  //record right[high-mid];
  record *left = malloc(sizeof(record)*(mid-low+1));
  record *right = malloc(sizeof(record)*(high-mid));

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
  free(left);
  free(right);
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
  total_records = new_arg->total_records;
  int low = thread_part * (total_records / cpus); //index of first record in thread's section
  int high = (thread_part + 1) * (total_records / cpus) - 1;
  if(thread_part == (cpus - 1)){
    high+= total_records%cpus;
  }
  int mid = low + (high - low) / 2;

  if(low < high){ //make sure there isn't only one record in the section
    merge_sort_more(low, mid);
    merge_sort_more(mid + 1, high);
    merge(low, mid, high);
  }

  return (void *)0;
}

int main(int argc, char *argv[]){
  if(argc != 3){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  int fd = open(argv[1],O_RDONLY);
  if(fd<0){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  struct stat s;
  int status = fstat(fd,&s);
  if(status < 0){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  size_t size = s.st_size;
  char *address;
  address = (char *)mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0);
  if(address == MAP_FAILED){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  close(fd);

  int total_records = size/100; //num of records in file (total bytes/100)

  mapping = (record *)malloc(total_records*sizeof(record));
  record *curr = mapping;

  //starts at first address in mapping and increments by size of record
  for(char *val = address; val < address + size; val+=100){
    curr->key = *(int *)val; //gets first int at address val
    memcpy(curr->value,val+sizeof(int),96);
    curr++;
  }
  munmap(address,size);

  cpus = total_records < THREAD_MAX ? total_records : THREAD_MAX;
  if(total_records > THREAD_MAX){
  f_data *file_d;
  file_d = malloc(sizeof(f_data)); //not sure if we need malloc but prob since we are sharing between threads
  file_d->total_records = total_records;
  pthread_t *threads = malloc(sizeof(pthread_t) * cpus);
  int i;
  for(i=0; i<cpus; i++){
    pthread_create(&threads[i], NULL, merge_sort, (void *)file_d); //can only send pointers to the function so send a struct with all the info we need
  }

  for(i=0; i<cpus; i++){
    pthread_join(threads[i],NULL);
  }

  merge_sections(cpus,1,total_records);
  free(threads);
  }
  else{
    merge_sort_more(0,total_records-1);
  }

  //WRITE TO OUTPUT FILE
  fd = open(argv[2], O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
  if(fd<0){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  lseek(fd, size-1, SEEK_SET);
  int w = write(fd,"",1);
  if(w != 1){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  char *output_file;
  output_file = (char *)mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
  if(address == MAP_FAILED){
    fprintf(stderr,"An error has occurred\n");
    exit(0);
  }
  // char * records_array = (char *)&mapping[0]; //initialize to first thread in array of structs
  // for(int i=0; i<size; i++){
  //   output_file[i] = records_array[i];
  // }
  memcpy(output_file, mapping, size);
  msync(output_file,size,MS_SYNC);
  munmap(output_file,size);
  fsync(fd);
  close(fd);

  /*int count = 0;
  while(count < 10){
    printf("key: %d\n",mapping[count].key);
    count++;
  }*/

  pthread_exit(NULL);
  free(mapping);
  return 0;
}

void merge_sections(int num, int sect_size, int total_records){
  for(int i=0; i < num; i+=2){
    int low = (i * (total_records/THREAD_MAX)) * sect_size; //lowest index
    int high = (i+2) *(total_records/THREAD_MAX) * sect_size  - 1; //high index
    int mid = low + (total_records/THREAD_MAX) * sect_size - 1; //midlle index
    if(high >= total_records){
      high = total_records - 1;
    }
    merge(low,mid,high);
  }
  if(num / 2 >= 1){
    merge_sections(num/2,sect_size*2,total_records);
  }
}
