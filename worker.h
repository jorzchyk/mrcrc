#include <stdint.h>
#include <sys/types.h>

typedef uint64_t count_t;

int worker(int i, int n, uint8_t* input, off_t inputsize, count_t* output);
