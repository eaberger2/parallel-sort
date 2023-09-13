# parallel-sort
Merge sorts key value pairs with multiple threads and semaphores to protect variables.

The majority of the sorting is done in the psort.c file. This program was run on a linux machine and uses GNU extensions such as get_nprocs.

Compile
`gcc c -o psort psort.c`

Run
`psort`
