#ifndef	__OS_APPLE_ENDIAN_H__
#define	__OS_APPLE_ENDIAN_H__

#ifndef __APPLE__
#error "This header file (endian.h) is MacOS X specific.\n"
#endif	/* __APPLE__ */

#include <libkern/OSByteOrder.h>

#define OS_htobe16(x) OSSwapHostToBigInt16(x)
#define OS_htole16(x) OSSwapHostToLittleInt16(x)
#define OS_be16toh(x) OSSwapBigToHostInt16(x)
#define OS_le16toh(x) OSSwapLittleToHostInt16(x)

#define OS_htobe32(x) OSSwapHostToBigInt32(x)
#define OS_htole32(x) OSSwapHostToLittleInt32(x)
#define OS_be32toh(x) OSSwapBigToHostInt32(x)
#define OS_le32toh(x) OSSwapLittleToHostInt32(x)

#define OS_htobe64(x) OSSwapHostToBigInt64(x)
#define OS_htole64(x) OSSwapHostToLittleInt64(x)
#define OS_be64toh(x) OSSwapBigToHostInt64(x)
#define OS_le64toh(x) OSSwapLittleToHostInt64(x)

#endif
