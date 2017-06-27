#include "common.h"
#include <unistd.h>

int fd;
int errno;
const int MY_LEN = 100;
Shared* shared_mem;


int setup_shared_memory(){
    fd = shm_open(MY_SHM, O_CREAT | O_RDWR, 0666);
    if(fd == -1){
        printf("shm_open() failed\n");
        exit(1);
    }
    ftruncate(fd, sizeof(Shared) + MY_LEN*sizeof(int));
}

int attach_shared_memory(){
    shared_mem = (Shared*)  mmap(NULL, sizeof(Shared) + MY_LEN*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(1);
    }
    return 0;
}

int init_shared_memory() {
    shared_mem->data = 0;
    sem_init(&(shared_mem->binary), 1, 1);
}

int main() {
    setup_shared_memory();
    attach_shared_memory();

    init_shared_memory();
    //shared_mem->arr = (int*) &shared_mem->arr;

    int i;
    for (i=0; i < MY_LEN; i++) {
        shared_mem->arr[i] = MY_LEN - i;
        printf("set %d\n", i);
    }

    while (1) {}

    return 0;
}
