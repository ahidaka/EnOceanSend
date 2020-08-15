//
// Utils.h
//

#pragma once

#include "typedefs.h"
//
#define IF_EXISTS_FREE(a) if ((a) != NULL) { free(a); (a) = NULL;}
//
extern void *MemDup(IN void *src, IN size_t len);
extern void DataDump(BYTE *Data, INT Len);
extern ULONG GetBits(IN BYTE *inArray, IN int start, IN int len);
extern double CalcA(double x1, double y1, double x2, double y2);
extern double CalcB(double x1, double y1, double x2, double y2);
extern BYTE Crc8CheckEx(BYTE *data, size_t offset, size_t count);
extern BYTE Crc8Check(BYTE *data, size_t count);

