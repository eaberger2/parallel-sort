#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

int
main(){
  printf("Processors online: %d\n",get_nprocs());
  printf("Total processors: %d\n",get_nprocs_conf());
}
