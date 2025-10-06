#ifndef UTF8_CODEC_H
#define UTF8_CODEC_H

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include<cstdint>

int UTF8Encode(uint32_t codepoint, unsigned char utf8_code[4]);
int UTF8Decode(const unsigned char* utf8_str, int offset, int len, uint32_t& codepoint);

#endif