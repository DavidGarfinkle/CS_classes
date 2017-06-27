#include "common.h"

int fd;
Shared* shared_mem;

int setup_shared_memory(){
    fd = shm_open(MY_SHM, O_RDWR, 0666);
    if(fd == -1){
        printf("shm_open() failed\n");
        exit(1);
    }
}

int attach_shared_memory(){
    shared_mem = (Shared*) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(1);
    }

    return 0;
}

int main() {
    setup_shared_memory();
    attach_shared_memory();

    int i;
    for (i=0; i < 100; i++) {
        if (shared_mem->arr[i] != 100 - i) {
            printf("error in array\n");
        }
    }

    return 0;
}
