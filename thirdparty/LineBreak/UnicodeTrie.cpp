//Original code from https://github.com/foliojs/unicode-trie, MIT License. Ported to C++

#include "UnicodeTrie.h"
#include "TinyInflate.h"
#include "SwapEndian.h"

// Shift size for getting the index-1 table offset.
#define SHIFT_1 (6 + 5)

// Shift size for getting the index-2 table offset.
#define SHIFT_2 5

// Difference between the two shift sizes,
// for getting an index-1 offset from an index-2 offset. 6=11-5
#define SHIFT_1_2 (SHIFT_1 - SHIFT_2)

// Number of index-1 entries for the BMP. 32=0x20
// This part of the index-1 table is omitted from the serialized form.
#define OMITTED_BMP_INDEX_1_LENGTH (0x10000 >> SHIFT_1)

// Number of entries in an index-2 block. 64=0x40
#define INDEX_2_BLOCK_LENGTH (1 << SHIFT_1_2)

// Mask for getting the lower bits for the in-index-2-block offset. */
#define INDEX_2_MASK (INDEX_2_BLOCK_LENGTH - 1)

// Shift size for shifting left the index array values.
// Increases possible data size with 16-bit index values at the cost
// of compactability.
// This requires data blocks to be aligned by DATA_GRANULARITY.
#define INDEX_SHIFT 2

// Number of entries in a data block. 32=0x20
#define DATA_BLOCK_LENGTH (1 << SHIFT_2)

// Mask for getting the lower bits for the in-data-block offset.
#define DATA_MASK (DATA_BLOCK_LENGTH - 1)

// The part of the index-2 table for U+D800..U+DBFF stores values for
// lead surrogate code _units_ not code _points_.
// Values for lead surrogate code _points_ are indexed with this portion of the table.
// Length=32=0x20=0x400>>SHIFT_2. (There are 1024=0x400 lead surrogates.)
#define LSCP_INDEX_2_OFFSET (0x10000 >> SHIFT_2)
#define LSCP_INDEX_2_LENGTH (0x400 >> SHIFT_2)

// Count the lengths of both BMP pieces. 2080=0x820
#define INDEX_2_BMP_LENGTH (LSCP_INDEX_2_OFFSET + LSCP_INDEX_2_LENGTH)

// The 2-byte UTF-8 version of the index-2 table follows at offset 2080=0x820.
// Length 32=0x20 for lead bytes C0..DF, regardless of SHIFT_2.
#define UTF8_2B_INDEX_2_OFFSET INDEX_2_BMP_LENGTH
#define UTF8_2B_INDEX_2_LENGTH (0x800 >> 6)  // U+0800 is the first code point after 2-byte UTF-8

// The index-1 table, only used for supplementary code points, at offset 2112=0x840.
// Variable length, for code points up to highStart, where the last single-value range starts.
// Maximum length 512=0x200=0x100000>>SHIFT_1.
// (For 0x100000 supplementary code points U+10000..U+10ffff.)
//
// The part of the index-2 table for supplementary code points starts
// after this index-1 table.
//
// Both the index-1 table and the following part of the index-2 table
// are omitted completely if there is only BMP data.
#define INDEX_1_OFFSET (UTF8_2B_INDEX_2_OFFSET + UTF8_2B_INDEX_2_LENGTH)

// The alignment size of a data block. Also the granularity for compaction.
#define DATA_GRANULARITY (1 << INDEX_SHIFT)



UnicodeTrie::UnicodeTrie(uint8_t* data) {
    // read binary format
    this->high_start = reinterpret_cast<uint32_t*>(data)[0];
    this->error_value = reinterpret_cast<uint32_t*>(data)[1];
    this->length = reinterpret_cast<uint32_t*>(data)[2];

    // double inflate the actual trie data
    uint8_t* inflated_data = new uint8_t[length];
    tinf_uncompress(data + 12, inflated_data);
    uint8_t* inflated_data_2 = new uint8_t[length];
    tinf_uncompress(inflated_data, inflated_data_2);

    length /= 4;

    // swap bytes from little-endian
    if (endian.is_big)
        SwapEndianUInt32(reinterpret_cast<uint32_t*>(inflated_data_2), length);

    this->data = reinterpret_cast<uint32_t*>(inflated_data_2);
    delete[] inflated_data;
}

uint32_t UnicodeTrie::Get(uint32_t codepoint) {
    uint32_t index;
    if ((codepoint < 0) || (codepoint > 0x10ffff)) {
        return this->error_value;
    }

    if ((codepoint < 0xd800) || ((codepoint > 0xdbff) && (codepoint <= 0xffff))) {
        // Ordinary BMP code point, excluding leading surrogates.
        // BMP uses a single level lookup.  BMP index starts at offset 0 in the index.
        // data is stored in the index array itself.
        index = (this->data[codepoint >> SHIFT_2] << INDEX_SHIFT) + (codepoint & DATA_MASK);
        return this->data[index];
    }

    if (codepoint <= 0xffff) {
        // Lead Surrogate Code Point.  A Separate index section is stored for
        // lead surrogate code units and code points.
        //   The main index has the code unit data.
        //   For this function, we need the code point data.
        index = (this->data[LSCP_INDEX_2_OFFSET + ((codepoint - 0xd800) >> SHIFT_2)] << INDEX_SHIFT) + (codepoint & DATA_MASK);
        return this->data[index];
    }

    if (codepoint < this->high_start) {
        // Supplemental code point, use two-level lookup.
        index = this->data[(INDEX_1_OFFSET - OMITTED_BMP_INDEX_1_LENGTH) + (codepoint >> SHIFT_1)];
        index = this->data[index + ((codepoint >> SHIFT_2) & INDEX_2_MASK)];
        index = (index << INDEX_SHIFT) + (codepoint & DATA_MASK);
        return this->data[index];
    }

    return this->data[this->length - DATA_GRANULARITY];
}


UnicodeTrie::~UnicodeTrie() {
    delete[] this->data;
}