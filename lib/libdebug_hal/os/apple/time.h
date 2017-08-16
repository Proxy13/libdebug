#ifndef	__OS_APPLE_TIME_H__
#define	__OS_APPLE_TIME_H__

#include <mach/mach_time.h>
#include <sys/time.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
#define CLOCK_MONOTONIC_FAST 0

static inline int
OS_clock_gettime(int clk_id, struct timespec *t)
{
	static int inited = 0;
	static mach_timebase_info_data_t timebase;
	uint64_t time;
	double nsec, sec;

	/* XXX TODO: properly serialise across threads! */
	if (inited == 0) {
		mach_timebase_info(&timebase);
		inited = 1;
	}

	/* XXX apparently this is 1:1 for x86, and overflows for ppc, sigh */
	time = mach_absolute_time();
#if 0
	nsec = ((double)time * (double)timebase.numer)/((double)timebase.denom);
	sec = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
#else
	sec = time / 1000000000ULL;
	nsec = time % 1000000000ULL;
#endif
	t->tv_sec = sec;
	t->tv_nsec = nsec;
	return (0);
}

#endif	/* __OS_APPLE_TIME_H__ */
