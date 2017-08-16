#ifndef	__OS_POSIX_ENDIAN_H__
#define	__OS_POSIX_ENDIAN_H__

#define OS_htobe16(x) htobe16(x)
#define OS_htole16(x) htole16(x)
#define OS_be16toh(x) be16toh(x)
#define OS_le16toh(x) le16toh(x)

#define OS_htobe32(x) htobe32(x)
#define OS_htole32(x) htole32(x)
#define OS_be32toh(x) be32toh(x)
#define OS_le32toh(x) le32toh(x)

#define OS_htobe64(x) htobe64(x)
#define OS_htole64(x) htole64(x)
#define OS_be64toh(x) be64toh(x)
#define OS_le64toh(x) le64toh(x)

#endif
