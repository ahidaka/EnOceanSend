//
// typedefs.h
//

#if defined(TYPEDEFS_H)
#else
#define TYPEDEFS_H

//
//

#define UNUSED_VARIABLE(x) (void)(x)
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define IN     /* input parameter */
#define OUT    /* output parameter */
#define INOUT  /* input/output parameter */
#define IN_OPT     /* input parameter, optional */
#define OUT_OPT    /* output parameter, optional */
#define INOUT_OPT  /* input/output parameter, optional */

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif
#define ON (1)
#define OFF (0)
#define YES (1)
#define NO (0)
#define ENABLE (1)
#define DISABLE (0)

typedef void VOID;
#ifdef BOOL
#undef BOOL
#endif
typedef int BOOL;
typedef char CHAR;
typedef unsigned char BYTE;
typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef int INT;
typedef unsigned int UINT;
#ifdef DWORD
#undef DWORD
#endif
#ifndef UINT_PTR // To avoid conflict from ETK WindowsDefs.h
typedef unsigned int DWORD;
#endif
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned long long ULONG64;
typedef float FLOAT;
typedef double DOUBLE;
#ifdef UINT64
#undef UINT64
typedef unsigned long long UINT64;
#endif
#endif
