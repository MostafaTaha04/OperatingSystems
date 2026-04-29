#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    // 1. Print current memory size
    printf("Memory size before allocation: %d bytes\n", memsize());

    // 2. Allocate 20k bytes of memory
    void *ptr = malloc(20000);
    if(ptr == 0){
        printf("malloc failed\n");
        exit(1);
    }

    // 3. Print memory size after allocation
    printf("Memory size after allocation: %d bytes\n", memsize());

    // 4. Free the allocated array
    free(ptr);

    // 5. Print memory size after release
    printf("Memory size after freeing: %d bytes\n", memsize());

    exit(0);
}