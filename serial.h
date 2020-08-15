//
// serial.h
//
typedef unsigned char byte;
typedef unsigned short ushort;
typedef unsigned long ulong;

//
//
#include "esp3.h"

#define _PD_FULLDATA (2)
#define _PD_VERBOSE  (3)
#define _PD_NOISY    (4)
#define _PD_SUPER_NOISY (5)

VOID PacketDebug(INT flag);
VOID PacketDump(BYTE *p);
INT PacketAnalyze(BYTE *p);
ULONG SystemMSec(VOID);
RETURN_CODE GetPacket(INT Fd, BYTE *Packet, USHORT BufferLength);
