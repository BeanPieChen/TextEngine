//Original code from https://github.com/foliojs/unicode-trie, MIT License. Ported to C++

#ifndef UNICODE_TRIE_H
#define UNICODE_TRIE_H
#include <cstdint>

class UnicodeTrie {
    uint32_t* data;
    uint32_t high_start;
    uint32_t error_value;
    uint32_t length;

public:
    UnicodeTrie(uint8_t* data);
    uint32_t Get(uint32_t codepoint);
    ~UnicodeTrie();
};

#endif