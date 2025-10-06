//Original code from https://github.com/foliojs/linebreak, MIT License. Ported to C++


#define DI_BRK  0 // Direct break opportunity
#define IN_BRK  1 // Indirect break opportunity
#define CI_BRK  2 // Indirect break opportunity for combining marks
#define CP_BRK  3 // Prohibited break for combining marks
#define PR_BRK  4 // Prohibited break

extern int pair_table[][33];