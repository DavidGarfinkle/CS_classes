#include "common.h"
#include <unistd.h>

int fd;
int errno;
Shared* shared_mem;

int setup_shared_memory(){
    fd = shm_open(MY_SHM, O_CREAT | O_RDWR, 0666);
    if(fd == -1){
        printf("shm_open() failed\n");
        exit(1);
    }
    ftruncate(fd, sizeof(Shared));
}

int attach_shared_memory(){
    shared_mem = (Shared*)  mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
    
    while (1) {
        sem_wait(&shared_mem->binary);
        printf("changing data from %d\n", shared_mem->data);
        shared_mem->data = shared_mem->data + 1;
        sleep(1);
        shared_mem->data = shared_mem->data * 2;
        printf("to %d\n", shared_mem->data);
        sem_post(&shared_mem->binary);
        sleep(1);
    }

    return 0;
}
