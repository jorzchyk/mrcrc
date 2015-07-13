#include <stdint.h>

#define DISTINCT_CRC_VALUES 0x10000

uint16_t gen_crc16(const uint8_t *data, uint16_t size);
