#ifndef	__OS_TIME_H__
#define	__OS_TIME_H__

/*
 * Since this library may end up being included by other things that
 * implement their own POSIX shims, let's avoid namespace clashes.
 */

#ifdef __APPLE__
#include "os/apple/time.h"
#else
#include <sys/time.h>

#define	OS_clock_gettime	clock_gettime

#endif

#endif
