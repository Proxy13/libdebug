/*-
 * Copyright (c) 2013 Netflix, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Netflix, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	__LIBIAPP_DEBUG_H__
#define	__LIBIAPP_DEBUG_H__

#define	DEBUG_SECTION_MAX		256
#define	DEBUG_TYPE_MAX			3
#define	DEBUG_SECTION_INVALID		0

#if 0
/* XXX are these needed? */
struct debug_instance;
struct debug_entry;
#endif

typedef enum {
        DEBUG_TYPE_PRINT,
        DEBUG_TYPE_LOG,
        DEBUG_TYPE_SYSLOG,
} debug_type_t;

typedef int debug_section_t;
typedef uint64_t debug_mask_t;

#define	DEBUG_MASK_ALL	(uint64_t) 0xffffffffffffffffULL

#define	DEBUG_SECTION_UNINIT	-1

extern	char *debug_level_strs[DEBUG_SECTION_MAX];
extern	debug_mask_t debug_levels[DEBUG_TYPE_MAX][DEBUG_SECTION_MAX];

extern	void debug_init(const char *progname);
extern	void debug_shutdown(void);
extern	void debug_setlevel_default(debug_type_t t, debug_mask_t m);
extern	void debug_setlevel_maskcopy(debug_type_t st, debug_type_t dt,
	    debug_mask_t ma, debug_mask_t mo);
extern	void debug_setlevel_mask(debug_type_t st, debug_mask_t ma,
	    debug_mask_t mo);
extern	debug_section_t debug_register(const char *dbgname);
extern	void debug_setmask(debug_section_t s, debug_type_t t,
	    debug_mask_t mask);
extern	void debug_setmask_str(const char *dbg, debug_type_t t,
	    debug_mask_t mask);
extern	void debug_syslog_enable(void);
extern	void debug_syslog_disable(void);

extern	void debug_set_filename(const char *filename);
extern	void debug_file_open(void);
extern	void debug_file_close(void);
extern	void debug_file_reopen(void);

extern	void do_debug(int section, debug_mask_t mask, const char *fmt, ...)
	    __attribute__ ((format (printf, 3, 4)));
extern	void do_debug_warn(int section, int xerrno, const char *fmt, ...)
	    __attribute__ ((format (printf, 3, 4)));

/*
 * This system currently uses a debug mask (up to 64 bits per section)
 * rather than a debug level.
 *
 * This is so parts of the debug area can be turned on/off as needed,
 * and debug entries can just tag themselves with multiple mask bits
 * if they want to trigger on multiple things.
 */

#define	DEBUG_LVL_DEBUG		0x00000001
#define	DEBUG_LVL_INFO		0x00000002
#define	DEBUG_LVL_NOTICE	0x00000004
#define	DEBUG_LVL_WARNING	0x00000008
#define	DEBUG_LVL_ERR		0x00000010
#define	DEBUG_LVL_CRIT		0x00000020
#define	DEBUG_LVL_ALERT		0x00000040
#define	DEBUG_LVL_EMERG		0x00000080

/*
 * The rest of the bitmap space is available for debug sections to
 * define as they wish.
 */

#if 1
/*
 * XXX TODO: always log DEBUG_LVL_EMERG!
 */
#define	DEBUG(s, l, m, ...)						\
	do {			\
		if ((debug_levels[DEBUG_TYPE_PRINT][(s)] & (l)) ||	\
		     (debug_levels[DEBUG_TYPE_LOG][(s)] & (l)) ||	\
		     (debug_levels[DEBUG_TYPE_SYSLOG][(s)] & (l)))	\
			do_debug(s, l, m, __VA_ARGS__);			\
	} while (0)

#else
#define DEBUG(s, l, m, ...)
#endif

/*
 * For now, warnings always generate debug info.
 */
#define	DEBUG_WARN(s, ...)						\
	do {								\
		do_debug_warn(s, errno, __VA_ARGS__);			\
	} while (0)

#endif	/* __LIBIAPP_DEBUG_H__ */
