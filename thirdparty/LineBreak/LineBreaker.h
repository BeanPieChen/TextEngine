//Original code from https://github.com/foliojs/linebreak , MIT License. Ported to C++

#ifndef LINE_BREAKER_H
#define LINE_BREAKER_H

#include <cstddef>
#include <cstdint>

class LineBreaker {
public:
    typedef uint32_t (*CodepointAt)(const void* cps, size_t index);

private:
    const void* cps;
    CodepointAt at_f;
    size_t i = 0;
    size_t last = 0;
    size_t len;
    int cur_class = -1;
    int next_class = -1;
    bool LB8a = false;
    bool LB21a = false;
    int LB30a = 0;

    uint32_t NextCodePoint();
    uint8_t NextCharClass();
    bool GetSimpleBreak();
    bool GetPairTableBreak(int last_class);

public:
    struct Break {
        size_t position;
        bool required = false;
    };

    LineBreaker(const void* cps, size_t len, CodepointAt at_f);
    bool NextBreak(Break& ret);
};


void LineBreakInit();
void LineBreakExit();

#endif // !LINE_BREAKER_H