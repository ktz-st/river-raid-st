#ifndef RR_BANK_H
#define RR_BANK_H
#include <godlib/base/base.h>
typedef struct { S16 x, y, scroll; U16 groups, height; } RRBankQuery;
U16 RR_Bank(const RRBankQuery *q, const U16 *mask, const U16 *screen);
U16 RR_BankReference(const RRBankQuery *q, const U16 *mask, const U16 *screen);
U16 RR_BankCPU(const U16 *mask, const U16 *line, U16 shift, U16 height, U16 groups);
#endif
