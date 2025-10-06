//Original code from https://github.com/foliojs/linebreak , MIT License. Ported to C++

#include "LineBreaker.h"
#include "LineBreakClasses.h"
#include "TinyInflate.h"
#include "Pairs.h"
#include "UnicodeTrie.h"

extern uint8_t classes_trie_data[];

UnicodeTrie* class_trie = nullptr;

uint8_t MapClass(uint8_t c) {
  switch (c) {
    case LBC_AI:
      return LBC_AL;

    case LBC_SA:
    case LBC_SG:
    case LBC_XX:
      return LBC_AL;

    case LBC_CJ:
      return LBC_NS;

    default:
      return c;
  }
}


uint8_t MapFirst(uint8_t c) {
  switch (c) {
    case LBC_LF:
    case LBC_NL:
      return LBC_BK;

    case LBC_SP:
      return LBC_WJ;

    default:
      return c;
  }
}


uint32_t LineBreaker::NextCodePoint() {
    const uint32_t code = this->at_f(this->cps, this->i);
    ++this->i;
    return code;
}


uint8_t LineBreaker::NextCharClass() {
    return MapClass(class_trie->Get(this->NextCodePoint()));
}


bool LineBreaker::GetSimpleBreak() {
    // handle classes not handled by the pair table
    switch (this->next_class) {
    case LBC_SP:
        return false;

    case LBC_BK:
    case LBC_LF:
    case LBC_NL:
        this->cur_class = LBC_BK;
        return false;

    case LBC_CR:
        this->cur_class = LBC_CR;
        return false;
    }

    return true;
}


bool LineBreaker::GetPairTableBreak(int last_class) {
    // if not handled already, use the pair table
    bool should_break = false;
    switch (pair_table[this->cur_class][this->next_class]) {
    case DI_BRK: // Direct break
        should_break = true;
        break;

    case IN_BRK: // possible indirect break
        should_break = (last_class == LBC_SP);
        break;

    case CI_BRK:
        should_break = (last_class == LBC_SP);
        if (!should_break) {
            should_break = false;
            return should_break;
        }
        break;

    case CP_BRK: // prohibited for combining marks
        if (last_class != LBC_SP)
            return should_break;
        break;

    case PR_BRK:
        break;
    }

    if (this->LB8a) {
        should_break = false;
    }

    // Rule LB21a
    if (this->LB21a && (this->cur_class == LBC_HY || this->cur_class == LBC_BA)) {
        should_break = false;
        this->LB21a = false;
    }
    else
        this->LB21a = (this->cur_class == LBC_HL);


    // Rule LB30a
    if (this->cur_class == LBC_RI) {
        this->LB30a++;
        if (this->LB30a == 2 && (this->next_class == LBC_RI)) {
            should_break = true;
            this->LB30a = 0;
        }
    }
    else
        this->LB30a = 0;

    this->cur_class = this->next_class;

    return should_break;
}


LineBreaker::LineBreaker(const void* cps, size_t len, CodepointAt at_f) :
    cps(cps),
    len(len),
    at_f(at_f)
{}


bool LineBreaker::NextBreak(Break& ret) {
    // get the first char if we're at the beginning of the string
    if (this->cur_class == -1) {
        int first_class = this->NextCharClass();
        this->cur_class = MapFirst(first_class);
        this->next_class = first_class;
        this->LB8a = (first_class == LBC_ZWJ);
        this->LB30a = 0;
    }

    while (this->i < this->len) {
        this->last = this->i;
        const int last_class = this->next_class;
        this->next_class = this->NextCharClass();

        // explicit newline
        if ((this->cur_class == LBC_BK) || ((this->cur_class == LBC_CR) && (this->next_class != LBC_LF))) {
            this->cur_class = MapFirst(MapClass(this->next_class));
            ret.position = this->last;
            ret.required = true;
            return true;
        }

        bool should_break = this->GetSimpleBreak();

        if (should_break)
            should_break = this->GetPairTableBreak(last_class);

        // Rule LB8a
        this->LB8a = (this->next_class == LBC_ZWJ);

        if (should_break) {
            ret.position = this->last;
            ret.required = false;
            return true;
        }
    }

    if (this->last < this->len) {
        this->last = this->len;
        ret.position = this->len;
        ret.required = false;
        return true;
    }

    return false;
}


void LineBreakInit() {
    tinf_init();
    class_trie = new UnicodeTrie(classes_trie_data);
}


void LineBreakExit() {
    delete class_trie;
}