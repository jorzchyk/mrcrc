#include <atomic_ops.h>
#include <err.h>
#include "crc16.h"
#include "worker.h"

#define BLOCK 4096

int worker(int i, int n, uint8_t* input, off_t inputsize, count_t* output)
{
	uint8_t* ptr = input + i*BLOCK;
	uint8_t* end = input + inputsize;
	uint16_t crc;
	
	while(ptr < end) {
		int len = ptr + BLOCK < end ? BLOCK : end - ptr;
		crc = gen_crc16(ptr, len);
		AO_fetch_and_add1_release_write(output + crc);
		ptr += n*BLOCK;
	}

	return 0;
}
