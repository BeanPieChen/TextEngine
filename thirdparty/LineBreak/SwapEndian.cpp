#include "SwapEndian.h"


Endian::Endian() {
	uint32_t a = 0x12345678U;
	uint8_t* b = reinterpret_cast<uint8_t*>(&a);
	if (*b == 0x12U)
		this->is_big = true;
}


Endian endian;


void SwapByte(uint8_t* bytes, size_t a, size_t b) {
	uint8_t temp = bytes[a];
	bytes[a] = bytes[b];
	bytes[b] = temp;
}


void SwapEndianUInt32(uint32_t* array, size_t len) {
	for (size_t i = 0;i < len;++i) {
		SwapByte(reinterpret_cast<uint8_t*>(array + i), 0, 3);
		SwapByte(reinterpret_cast<uint8_t*>(array + i), 1, 2);
	}
}