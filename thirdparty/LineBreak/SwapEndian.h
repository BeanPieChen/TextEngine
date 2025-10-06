#pragma once
#include <cstdint>


class Endian {
public:
	bool is_big = false;
	Endian();
};


extern Endian endian;


void SwapEndianUInt32(uint32_t* array, size_t len);