//Original code from https://github.com/foliojs/tiny-inflate, MIT License. Ported to C++

#include "TinyInflate.h"

#define TINF_OK 0
#define TINF_DATA_ERROR -3

struct Tree{
    uint16_t table[16];
    uint16_t trans[288];
};


struct Data{
    uint8_t* source;
    int source_index=0;
    int tag=0;
    int bitcount=0;

    uint8_t* dest;
    int dest_len=0;

    Tree ltree;
    Tree dtree;

    Data(uint8_t* source,uint8_t*dest):source(source),dest(dest){}
};


Tree sltree;
Tree sdtree;


/* extra bits and base tables for length codes */
uint8_t length_bits[30];
uint16_t length_base[30];


/* extra bits and base tables for distance codes */
uint8_t dist_bits[30];
uint16_t dist_base[30];


/* special ordering of code length codes */
uint8_t clcidx[] = {
  16, 17, 18, 0, 8, 7, 9, 6,
  10, 5, 11, 4, 12, 3, 13, 2,
  14, 1, 15
};


/* used by tinf_decode_trees, avoids allocations every call */
Tree code_tree;
uint8_t lengths[288+32];


void tinf_build_bits_base(uint8_t bits[], uint16_t base[], int delta, int first) {
    int i, sum;

    /* build bits table */
    for (i = 0; i < delta; ++i) bits[i] = 0;
    for (i = 0; i < 30 - delta; ++i) bits[i + delta] = i / delta | 0;

    /* build base table */
    for (sum = first, i = 0; i < 30; ++i) {
        base[i] = sum;
        sum += 1 << bits[i];
    }
}


void tinf_build_fixed_trees(Tree& lt, Tree& dt) {
    int i;

    /* build fixed length tree */
    for (i = 0; i < 7; ++i) lt.table[i] = 0;

    lt.table[7] = 24;
    lt.table[8] = 152;
    lt.table[9] = 112;

    for (i = 0; i < 24; ++i) lt.trans[i] = 256 + i;
    for (i = 0; i < 144; ++i) lt.trans[24 + i] = i;
    for (i = 0; i < 8; ++i) lt.trans[24 + 144 + i] = 280 + i;
    for (i = 0; i < 112; ++i) lt.trans[24 + 144 + 8 + i] = 144 + i;

    /* build fixed distance tree */
    for (i = 0; i < 5; ++i) dt.table[i] = 0;

    dt.table[5] = 32;

    for (i = 0; i < 32; ++i) dt.trans[i] = i;
}


/* given an array of code lengths, build a tree */
uint16_t offs[16];

void tinf_build_tree(Tree& t, uint8_t lengths[], int off, int num) {
    int i, sum;

    /* clear code length count table */
    for (i = 0; i < 16; ++i) t.table[i] = 0;

    /* scan symbol lengths, and sum code length counts */
    for (i = 0; i < num; ++i) t.table[lengths[off + i]]++;

    t.table[0] = 0;

    /* compute offset table for distribution sort */
    for (sum = 0, i = 0; i < 16; ++i) {
        offs[i] = sum;
        sum += t.table[i];
    }

    /* create code->symbol translation table (symbols sorted by code) */
    for (i = 0; i < num; ++i) {
        if (lengths[off + i]) t.trans[offs[lengths[off + i]]++] = i;
    }
}


/* ---------------------- *
 * -- decode functions -- *
 * ---------------------- */

/* get one bit from source stream */
int tinf_getbit(Data& d) {
    /* check if tag is empty */
    if (!d.bitcount--) {
        /* load next tag */
        d.tag = d.source[d.source_index++];
        d.bitcount = 7;
    }

    /* shift bit out of tag */
    int bit = d.tag & 1;
    d.tag >>= 1;

    return bit;
}


/* read a num bit value from a stream and add base */
int tinf_read_bits(Data& d, int num, int base) {
    if (!num)
        return base;

    while (d.bitcount < 24) {
        d.tag |= d.source[d.source_index++] << d.bitcount;
        d.bitcount += 8;
    }

    int val = d.tag & (0xffffU >> (16 - num));
    d.tag >>= num;
    d.bitcount -= num;
    return val + base;
}


/* given a data stream and a tree, decode a symbol */
int tinf_decode_symbol(Data& d, Tree& t) {
    while (d.bitcount < 24) {
        d.tag |= d.source[d.source_index++] << d.bitcount;
        d.bitcount += 8;
    }

    int sum = 0, cur = 0, len = 0;
    int tag = d.tag;

    /* get more bits while code value is above sum */
    do {
        cur = 2 * cur + (tag & 1);
        tag >>= 1;
        ++len;

        sum += t.table[len];
        cur -= t.table[len];
    } while (cur >= 0);

    d.tag = tag;
    d.bitcount -= len;

    return t.trans[sum + cur];
}


/* given a data stream, decode dynamic trees from it */
void tinf_decode_trees(Data& d, Tree& lt, Tree& dt) {
    int hlit, hdist, hclen;
    int i, num, length;

    /* get 5 bits HLIT (257-286) */
    hlit = tinf_read_bits(d, 5, 257);

    /* get 5 bits HDIST (1-32) */
    hdist = tinf_read_bits(d, 5, 1);

    /* get 4 bits HCLEN (4-19) */
    hclen = tinf_read_bits(d, 4, 4);

    for (i = 0; i < 19; ++i) lengths[i] = 0;

    /* read code lengths for code length alphabet */
    for (i = 0; i < hclen; ++i) {
        /* get 3 bits code length (0-7) */
        int clen = tinf_read_bits(d, 3, 0);
        lengths[clcidx[i]] = clen;
    }

    /* build code length tree */
    tinf_build_tree(code_tree, lengths, 0, 19);

    /* decode code lengths for the dynamic trees */
    for (num = 0; num < hlit + hdist;) {
        int sym = tinf_decode_symbol(d, code_tree);
        int prev;
        switch (sym) {
        case 16:
            /* copy previous code length 3-6 times (read 2 bits) */
            prev = lengths[num - 1];
            for (length = tinf_read_bits(d, 2, 3); length; --length) {
                lengths[num++] = prev;
            }
            break;
        case 17:
            /* repeat code length 0 for 3-10 times (read 3 bits) */
            for (length = tinf_read_bits(d, 3, 3); length; --length) {
                lengths[num++] = 0;
            }
            break;
        case 18:
            /* repeat code length 0 for 11-138 times (read 7 bits) */
            for (length = tinf_read_bits(d, 7, 11); length; --length) {
                lengths[num++] = 0;
            }
            break;
        default:
            /* values 0-15 represent the actual code lengths */
            lengths[num++] = sym;
            break;
        }
    }

    /* build dynamic trees */
    tinf_build_tree(lt, lengths, 0, hlit);
    tinf_build_tree(dt, lengths, hlit, hdist);
}


/* ----------------------------- *
 * -- block inflate functions -- *
 * ----------------------------- */

/* given a stream and two trees, inflate a block of data */
int tinf_inflate_block_data(Data& d, Tree& lt, Tree& dt) {
    while (1) {
        int sym = tinf_decode_symbol(d, lt);

        /* check for end of block */
        if (sym == 256) {
            return TINF_OK;
        }

        if (sym < 256) {
            d.dest[d.dest_len++] = sym;
        }
        else {
            int length, dist, offs;
            int i;

            sym -= 257;

            /* possibly get more bits from length code */
            length = tinf_read_bits(d, length_bits[sym], length_base[sym]);

            dist = tinf_decode_symbol(d, dt);

            /* possibly get more bits from distance code */
            offs = d.dest_len - tinf_read_bits(d, dist_bits[dist], dist_base[dist]);

            /* copy match */
            for (i = offs; i < offs + length; ++i) {
                d.dest[d.dest_len++] = d.dest[i];
            }
        }
    }
}


/* inflate an uncompressed block of data */
int tinf_inflate_uncompressed_block(Data& d) {
    int length, invlength;
    int i;

    /* unread from bitbuffer */
    while (d.bitcount > 8) {
        d.source_index--;
        d.bitcount -= 8;
    }

    /* get length */
    length = d.source[d.source_index + 1];
    length = 256 * length + d.source[d.source_index];

    /* get one's complement of length */
    invlength = d.source[d.source_index + 3];
    invlength = 256 * invlength + d.source[d.source_index + 2];

    /* check length */
    if (length != (~invlength & 0x0000ffffU))
        return TINF_DATA_ERROR;

    d.source_index += 4;

    /* copy block */
    for (i = length; i; --i)
        d.dest[d.dest_len++] = d.source[d.source_index++];

    /* make sure we start next block on a byte boundary */
    d.bitcount = 0;

    return TINF_OK;
}


/* inflate stream from source to dest */
int tinf_uncompress(uint8_t* source, uint8_t* dest) {
    Data d(source, dest);
    int bfinal, btype, res;

    do {
        /* read final block flag */
        bfinal = tinf_getbit(d);

        /* read block type (2 bits) */
        btype = tinf_read_bits(d, 2, 0);

        /* decompress block */
        switch (btype) {
        case 0:
            /* decompress uncompressed block */
            res = tinf_inflate_uncompressed_block(d);
            break;
        case 1:
            /* decompress block with fixed huffman trees */
            res = tinf_inflate_block_data(d, sltree, sdtree);
            break;
        case 2:
            /* decompress block with dynamic huffman trees */
            tinf_decode_trees(d, d.ltree, d.dtree);
            res = tinf_inflate_block_data(d, d.ltree, d.dtree);
            break;
        default:
            res = TINF_DATA_ERROR;
        }

        if (res != TINF_OK)
            throw "Data error";

    } while (!bfinal);

    return d.dest_len;
}


/* -------------------- *
 * -- initialization -- *
 * -------------------- */

void tinf_init() {
    /* build fixed huffman trees */
    tinf_build_fixed_trees(sltree, sdtree);

    /* build extra bits and base tables */
    tinf_build_bits_base(length_bits, length_base, 4, 3);
    tinf_build_bits_base(dist_bits, dist_base, 2, 1);

    /* fix a special case */
    length_bits[28] = 0;
    length_base[28] = 258;
}