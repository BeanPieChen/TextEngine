//Original code from https://github.com/foliojs/linebreak, MIT License. Ported to C++

// The following break classes are handled by the pair table
#define LBC_CL  1   // Closing punctuation
#define LBC_OP  0   // Opening punctuation
#define LBC_CP  2   // Closing parenthesis
#define LBC_QU  3   // Ambiguous quotation
#define LBC_GL  4   // Glue
#define LBC_NS  5   // Non-starters
#define LBC_EX  6   // Exclamation/Interrogation
#define LBC_SY  7   // Symbols allowing break after
#define LBC_IS  8   // Infix separator
#define LBC_PR  9   // Prefix
#define LBC_PO  10  // Postfix
#define LBC_NU  11  // Numeric
#define LBC_AL  12  // Alphabetic
#define LBC_HL  13  // Hebrew Letter
#define LBC_ID  14  // Ideographic
#define LBC_IN  15  // Inseparable characters
#define LBC_HY  16  // Hyphen
#define LBC_BA  17  // Break after
#define LBC_BB  18  // Break before
#define LBC_B2  19  // Break on either side (but not pair)
#define LBC_ZW  20  // Zero-width space
#define LBC_CM  21  // Combining marks
#define LBC_WJ  22  // Word joiner
#define LBC_H2  23  // Hangul LV
#define LBC_H3  24  // Hangul LVT
#define LBC_JL  25  // Hangul L Jamo
#define LBC_JV  26  // Hangul V Jamo
#define LBC_JT  27  // Hangul T Jamo
#define LBC_RI  28  // Regional Indicator
#define LBC_EB  29  // Emoji Base
#define LBC_EM  30  // Emoji Modifier
#define LBC_ZWJ 31 // Zero Width Joiner
#define LBC_CB  32  // Contingent break

// The following break classes are not handled by the pair table
#define LBC_AI  33  // Ambiguous (Alphabetic or Ideograph)
#define LBC_BK  34  // Break (mandatory)
#define LBC_CJ  35  // Conditional Japanese Starter
#define LBC_CR  36  // Carriage return
#define LBC_LF  37  // Line feed
#define LBC_NL  38  // Next line
#define LBC_SA  39  // South-East Asian
#define LBC_SG  40  // Surrogates
#define LBC_SP  41  // Space
#define LBC_XX  42  // Unknown
