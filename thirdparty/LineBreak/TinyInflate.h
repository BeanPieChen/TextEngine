//Original code from https://github.com/foliojs/tiny-inflate, MIT License. Ported to C++

#ifndef TINY_INFLATE_H
#define TINY_INFLATE_H
#include <stdint.h>

int tinf_uncompress(uint8_t* source, uint8_t* dest);
void tinf_init();

#endif