#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    printf("--- Testing Task 0: PRNG ---\n");

    uint seed = uptime();
    printf("Setting seed to: %d\n", seed);
    lcg_srand(seed);

    for(int i = 0; i < 5; i++) {
        uint random_num = lcg_rand();
        printf("Random number %d: %d\n", i+1, random_num);
    }

    exit(0);
}