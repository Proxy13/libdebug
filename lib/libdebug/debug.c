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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>

#include "os/time.h"

#include <err.h>

#include <sys/time.h>
#include <sys/queue.h>

#include <pthread.h>

#include "debug.h"
#include "debug_internal.h"

/*
 * XXX TODO: these are likely global due to visibility by other
 * modules, but we could make them indirect through a singleton
 * later on.
 */
char * debug_level_strs[DEBUG_SECTION_MAX];
debug_mask_t debug_levels[DEBUG_TYPE_MAX][DEBUG_SECTION_MAX];
static debug_mask_t default_lvl_print = DEBUG_LVL_INFO | DEBUG_LVL_CRIT | DEBUG_LVL_ERR;
static debug_mask_t default_lvl_log = 0;
static debug_mask_t default_lvl_syslog = 0;

/*
 * XXX TODO: The locking is very simplistic for now.
 *
 * If there's a lot of logging going on then we will end up
 * lots of lock contention - which you can fix in a variety
 * of lock-free or lock-minimal ways.
 */

static struct debug_instance debugInstance;

debug_section_t
debug_register(const char *dbgname)
{
	int i;

	if (dbgname == NULL)
		return (-1);

	/* XXX locking */

	for (i = 0; i < DEBUG_SECTION_MAX; i++) {
		if (debug_level_strs[i] == NULL) {
			debug_level_strs[i] = strdup(dbgname);
			/* Default to logging info/err/crit to stderr */
			debug_levels[DEBUG_TYPE_PRINT][i] = default_lvl_print;
			debug_levels[DEBUG_TYPE_LOG][i] = default_lvl_log;
			debug_levels[DEBUG_TYPE_SYSLOG][i] = default_lvl_syslog;

			return (i);
		}
	}

	/*
	 * XXX should log to say, log type 0 (which should be "debug")
	 */
	fprintf(stderr, "%s: couldn't allocate debug level for '%s'\n",
	    __func__,
	    dbgname);
	return (-1);
}

void
debug_setlevel_default(debug_type_t t, debug_mask_t m)
{
	switch (t) {
	case DEBUG_TYPE_PRINT:
		default_lvl_print = m;
		break;
	case DEBUG_TYPE_LOG:
		default_lvl_log = m;
		break;
	case DEBUG_TYPE_SYSLOG:
		default_lvl_syslog = m;
		break;
	default:
		fprintf(stderr, "%s: unknown debug type (%d)\n",
		    __func__, t);
		break;
	}
}

/*
 * For all debug sections, mask in the source type from the
 * destination type with an optional AND and OR to do some filtering.
 */
void
debug_setlevel_maskcopy(debug_type_t st, debug_type_t dt, debug_mask_t ma,
    debug_mask_t mo)
{
	int i;
	debug_mask_t m;

	(void) pthread_mutex_lock(&debugInstance.debug_lock);
	for (i = 0; i < DEBUG_SECTION_MAX; i++) {
		if (debug_level_strs[i] == NULL)
			continue;
		m = debug_levels[st][i];
		m &= ma;
		m |= mo;
		debug_levels[dt][i] = m;
	}

	(void) pthread_mutex_unlock(&debugInstance.debug_lock);
}

/*
 * For all debug sections, do the following AND and OR values.
 * This allows for globally adding/removing flags as appropriate.
 */
void
debug_setlevel_mask(debug_type_t st, debug_mask_t ma, debug_mask_t mo)
{
	int i;
	debug_mask_t m;

	(void) pthread_mutex_lock(&debugInstance.debug_lock);
	for (i = 0; i < DEBUG_SECTION_MAX; i++) {
		if (debug_level_strs[i] == NULL)
			continue;
		m = debug_levels[st][i];
		m &= ma;
		m |= mo;
		debug_levels[st][i] = m;
	}

	(void) pthread_mutex_unlock(&debugInstance.debug_lock);
}
void
debug_setlevel(debug_section_t s, debug_type_t t, debug_mask_t mask)
{

	if (t >= DEBUG_TYPE_MAX)
		return;

	fprintf(stderr, "%s: setting section (%d) (type %d) = mask %llx\n",
	    __func__, s, t, (long long) mask);
	(void) pthread_mutex_lock(&debugInstance.debug_lock);
	debug_levels[t][s] = mask;
	(void) pthread_mutex_unlock(&debugInstance.debug_lock);
}

void
debug_set_filename(const char *filename)
{
	struct debug_instance *ds = &debugInstance;

	(void) pthread_mutex_lock(&ds->debug_file_lock);
	if (ds->debug_filename != NULL)
		free(ds->debug_filename);
	ds->debug_filename = strdup(filename);
	(void) pthread_mutex_unlock(&ds->debug_file_lock);
}

static void
debug_file_open_locked(struct debug_instance *ds)
{

	if (ds->debug_file != NULL) {
		fclose(ds->debug_file);
		ds->debug_file = NULL;
	}

	if (ds->debug_filename == NULL)
		return;

	ds->debug_file = fopen(ds->debug_filename, "a+");

	if (ds->debug_file == NULL) {
		/* XXX should debuglog this! */
		fprintf(stderr, "%s: fopen failed (%s): %s\n",
		    __func__,
		    ds->debug_filename,
		    strerror(errno));
	}
}

static void
debug_file_close_locked(struct debug_instance *ds)
{

	if (ds->debug_file == NULL) {
		return;
	}
	fflush(ds->debug_file);
	fclose(ds->debug_file);
	ds->debug_file = NULL;
}

/*
 * * This doesn't flush pending log entries before opening the file.
 * * Opening a file is a potentially blocking operation.
 */
void
debug_file_open(void)
{
	struct debug_instance *ds = &debugInstance;

	(void) pthread_mutex_lock(&debugInstance.debug_file_lock);
	debug_file_open_locked(ds);
	(void) pthread_mutex_unlock(&debugInstance.debug_file_lock);
}

/*
 * * This doesn't flush pending log entries before closing the file.
 * * Closing a file is a potentially blocking operation.
 */
void
debug_file_close(void)
{
	struct debug_instance *ds = &debugInstance;

	(void) pthread_mutex_lock(&debugInstance.debug_file_lock);
	debug_file_close_locked(ds);
	(void) pthread_mutex_unlock(&debugInstance.debug_file_lock);
}

/*
 * * This doesn't flush pending log entries before closing/opening the file.
 * * Reopening a file is a potentially blocking operation.
 */
void
debug_file_reopen(void)
{
	struct debug_instance *ds = &debugInstance;

	(void) pthread_mutex_lock(&debugInstance.debug_file_lock);
	debug_file_close_locked(ds);
	debug_file_open_locked(ds);
	(void) pthread_mutex_unlock(&debugInstance.debug_file_lock);
}

/*
 * Create a debug entry to put debug contents in before queuing.
 * This will always attempt to allocate; it's up to the caller
 * to rate limit for now.
 *
 * This happens with no locks held, for the above reason.
 */
static struct debug_entry *
debug_entry_create(void)
{
	struct debug_entry *d;

	d = malloc(sizeof(*d));
	if (d == NULL) {
		return (NULL);
	}
	bzero(&d->e, sizeof(d->e));
	return (d);
}

/*
 * Free a debug entry that has already been consumed from
 * whichever list owns it.
 */
static void
debug_entry_free(struct debug_entry *d)
{

	free(d);
}

/*
 * Queue a debug entry.  This will always succeed.
 *
 * The lock must be held.
 */
static void
debug_entry_queue_locked(struct debug_instance *ds, struct debug_entry *de)
{

	TAILQ_INSERT_TAIL(&ds->list, de, e);
	ds->nitems++;
}

#if 0
/*
 * Dequeue a debug entry.  This will either succeed or return
 * NULL if there are no entries.
 *
 * The lock must be held.
 */
static struct debug_entry *
debug_entry_dequeue_locked(struct debug_instance *ds)
{
	struct debug_entry *de;

	de = TAILQ_FIRST(&ds->list);
	if (de == NULL)
		return (NULL);
	TAILQ_REMOVE(&ds->list, de, e);
	ds->nitems--;
	return (de);
}
#endif

/*
 * This is a very racy check.  Later on it should turn into
 * a "reserve" API and a "queue" API so we can do lock-free
 * atomic debug queue list reservations before we grab the
 * lock to queue it.
 */
static int
debug_instance_can_queue(struct debug_instance *ds)
{

	if (ds->nitems > ds->debug_queue_limit)
		return (0);
	return (1);
}

/*
 * Do an instance of writing a log entry.
 *
 * This for now does flushing but that kills performance!
 * Just keep that in mind!
 *
 * Return a bitmask showing which particular output streams
 * were written to, so appropriate flushing can occur.
 *
 * This must be called with the file_lock held.
 */
static int
debug_instance_log_entry_locked(struct debug_instance *ds, struct debug_entry *de)
{
	struct tm t, *tp;
	time_t tt;
	char tbuf[128];
	char buf[128];
	int ret = 0;

	/* Generate debug timestamp string */
	tt = de->tv.tv_sec;
	tp = localtime_r(&tt, &t);

	/* Normal HMS string */
	strftime(buf, 128, "%Y-%m-%d %H:%M:%S", tp);
	snprintf(tbuf, 128, "%s (%llu.%06llu)| ",
	    buf,
	    (unsigned long long) de->tv.tv_sec,
	    (unsigned long long) de->tv.tv_usec);

	/*
	 * Ok, now that it's done, we can figure out where to
	 * write it to.
	 */
	if (debug_levels[DEBUG_TYPE_PRINT][de->debug_section] & de->debug_mask) {
		fprintf(stderr, "%s%s", tbuf, de->buf);
		ret |= 1 << DEBUG_TYPE_PRINT;
	}
	if (ds->debug_file != NULL &&
	    debug_levels[DEBUG_TYPE_LOG][de->debug_section] & de->debug_mask) {
		fprintf(ds->debug_file, "%s%s", tbuf, de->buf);
		ret |= 1 << DEBUG_TYPE_LOG;
	}
	if (ds->debug_syslog_enable == 1 &&
	    debug_levels[DEBUG_TYPE_SYSLOG][de->debug_section] & de->debug_mask) {
		/* XXX TODO should map these levels into syslog levels */
		/* XXX TODO: syslog facility name, etc, etc */
		syslog(LOG_DEBUG, "%s%s", tbuf, de->buf);
		ret |= 1 << DEBUG_TYPE_SYSLOG;
	}

	return (ret);
}

static void
debug_instance_flush_locked(struct debug_instance *ds, int v)
{

	/* XXX TODO: Yes, should delay this all.. */
	if (v & (1 << DEBUG_TYPE_PRINT))
		fflush(stderr);
	if (v & (1 << DEBUG_TYPE_LOG))
		fflush(ds->debug_file);

}

/*
 * Called to do actual debugging.
 *
 * The macro hilarity is done so that the arguments to the debug
 * statement aren't actually evaluated unless the debugging level
 * is matched.
 */
void
do_debug(int section, debug_mask_t mask, const char *fmt, ...)
{
	va_list ap;
	struct timeval tv;
	struct debug_instance *ds = &debugInstance;
	struct debug_entry *de;

	/* XXX TODO: should log/count messages we've missed */
	if (! debug_instance_can_queue(ds)) {
		return;
	}

	/* Get wall clock timestamp */
	(void) gettimeofday(&tv, NULL);

	de = debug_entry_create();
	if (de == NULL) {
		/* XXX TODO: statistics */
		return;
	}

	/* Log the message itself */
	va_start(ap, fmt);
	vsnprintf(de->buf, 512, fmt, ap);
	va_end(ap);

	de->tv = tv;

	/* XXX TODO: bounds check these */
	de->debug_section = section;
	de->debug_mask = mask;

	/* Queue entry, wakeup worker thread */
	/* XXX TODO: yes, methodize this */
	(void) pthread_mutex_lock(&ds->debug_lock);
	debug_entry_queue_locked(ds, de);
	pthread_cond_signal(&ds->log_cond);
	(void) pthread_mutex_unlock(&ds->debug_lock);
}

/*
 * debugging - warn() wrapper.
 */
void
do_debug_warn(int section, int xerrno, const char *fmt, ...)
{
	va_list ap;
	struct timeval tv;
	char buf[512];
	debug_mask_t mask = DEBUG_LVL_ERR | DEBUG_LVL_CRIT;
	struct debug_instance *ds = &debugInstance;
	struct debug_entry *de;

	/* XXX TODO: should log/count messages we've missed */
	if (! debug_instance_can_queue(ds)) {
		return;
	}

	/* Get wall clock timestamp */
	(void) gettimeofday(&tv, NULL);

	de = debug_entry_create();
	if (de == NULL) {
		/* XXX TODO: statistics */
		return;
	}

	/* Log the message itself */
	va_start(ap, fmt);
	vsnprintf(buf, 512, fmt, ap);
	va_end(ap);

	/* And now, log the errno string */
	/* XXX TODO: use strerror_r() */
	snprintf(de->buf, 512, "%s: %s (%d)\n",
	    buf,
	    strerror(xerrno),
	    xerrno);

	de->tv = tv;

	/* XXX TODO: bounds check these */
	de->debug_section = section;
	de->debug_mask = mask;

	/* Queue entry, wakeup worker thread */
	/* XXX TODO: yes, methodize this */
	(void) pthread_mutex_lock(&ds->debug_lock);
	debug_entry_queue_locked(ds, de);
	pthread_cond_signal(&ds->log_cond);
	(void) pthread_mutex_unlock(&ds->debug_lock);
}

void
debug_setmask_str(const char *dbg, debug_type_t t, debug_mask_t mask)
{
	int i;
	int len;

	/* XXX locking */


	len = strlen(dbg);

	for (i = 0; i < DEBUG_SECTION_MAX; i++) {
		if (debug_level_strs[i] == NULL)
			break;
		if (strlen(debug_level_strs[i]) == len &&
		    strncmp(debug_level_strs[i], dbg, len) == 0)
			break;
	}
	fprintf(stderr, "%s: setting debug '%s' (%d) to %llx\n",
	    __func__, dbg, i, (unsigned long long) mask);
	if (i >= DEBUG_SECTION_MAX)
		return;		/* XXX return something useful? */

	debug_setlevel(i, t, mask);
}

int
debug_setmask_str2(const char *dbg, const char *dtype, debug_mask_t mask)
{
	int i;
	int len;
	int d_i, t_i;

	len = strlen(dbg);

	/* XXX locking */

	/* Section lookup */
	for (i = 0; i < DEBUG_SECTION_MAX; i++) {
		if (debug_level_strs[i] == NULL)
			break;
		if (strlen(debug_level_strs[i]) == len &&
		    strncmp(debug_level_strs[i], dbg, len) == 0)
			break;
	}
	if (i >= DEBUG_SECTION_MAX)
		return (-1);
	if (debug_level_strs[i] == NULL)
		return (-1);
	d_i = i;

	/* Type lookup */
	if (strncmp("syslog", dtype, 6) == 0) {
		t_i = DEBUG_TYPE_SYSLOG;
	} else if (strncmp("log", dtype, 4) == 0) {
		t_i = DEBUG_TYPE_LOG;
	} else if (strncmp("print", dtype, 4) == 0) {
		t_i = DEBUG_TYPE_PRINT;
	} else {
		return (-1);
	}

	fprintf(stderr, "%s: setting debug '%s' (%d) to %llx\n",
	    __func__, dbg, i, (unsigned long long) mask);

	debug_setlevel(d_i, t_i, mask);
	return (0);
}

static void *
debug_run_thread(void *arg)
{
        TAILQ_HEAD(, debug_entry) staging_list;
	struct debug_instance *ds = arg;
	struct debug_entry *de;
	struct timespec ts;
	int ret, r;

	pthread_mutex_lock(&ds->debug_lock);

	while (1) {
		r = 0;

		/* Only wait if the list is empty */
		if (ds->nitems == 0) {
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 5;
			ret = pthread_cond_timedwait(&ds->log_cond, &ds->debug_lock, &ts);
			if (ret == EWOULDBLOCK && ds->nitems == 0)
				continue;

			/* XXX handle error */
			if (ret != 0 && ds->nitems == 0)
				continue;
		}

		if (ds->debug_thr_do_exit) {
			pthread_mutex_unlock(&ds->debug_lock);
			return (NULL);
		}

		/* Take /all/ of the items under the queue lock */
		TAILQ_INIT(&staging_list);
		TAILQ_CONCAT(&staging_list, &ds->list, e);
		ds->nitems = 0;

		pthread_mutex_unlock(&ds->debug_lock);

		/* File IO goes here */
		pthread_mutex_lock(&ds->debug_file_lock);
		while (! TAILQ_EMPTY(&staging_list)) {
			de = TAILQ_FIRST(&staging_list);
			TAILQ_REMOVE(&staging_list, de, e);
			r |= debug_instance_log_entry_locked(ds, de);
			debug_entry_free(de);
		}

		/* Now, do deferred log flushing */
		debug_instance_flush_locked(ds, r);

		pthread_mutex_unlock(&ds->debug_file_lock);

		pthread_mutex_lock(&ds->debug_lock);
	}
}

static void
debug_init_instance(struct debug_instance *ds)
{
	int ret;

	bzero(ds, sizeof(*ds));

	ds->debug_queue_limit = 128;
	TAILQ_INIT(&ds->list);

	pthread_mutex_init(&ds->debug_lock, NULL);
	pthread_mutex_init(&ds->debug_file_lock, NULL);
	pthread_cond_init(&ds->log_cond, NULL);

	ret = pthread_create(&ds->log_thread, NULL,
	    debug_run_thread, ds);
	if (ret < 0) {
		err(1, "pthread_create");
	}
}

void
debug_init(const char *progname)
{

	debug_init_instance(&debugInstance);

	bzero(debug_level_strs, sizeof(debug_level_strs));
	bzero(debug_levels, sizeof(debug_levels));

	/* Enable syslog debugging by default */
	openlog(progname, LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_DAEMON);
	debugInstance.debug_syslog_enable = 1;
}

void
debug_syslog_enable(void)
{

	debugInstance.debug_syslog_enable = 1;
}

void
debug_syslog_disable(void)
{

	debugInstance.debug_syslog_enable = 0;
}

static void
debug_shutdown_instance(struct debug_instance *ds)
{

	/* Signal the worker thread to exit */
	pthread_mutex_lock(&ds->debug_lock);
	ds->debug_thr_do_exit = 1;
	pthread_cond_signal(&ds->log_cond);
	pthread_mutex_unlock(&ds->debug_lock);

	/* Exit worker thread */
	pthread_join(ds->log_thread, NULL);

	/* Close the log file, if open, which will do a final flush. */
	pthread_mutex_lock(&ds->debug_file_lock);
	debug_file_close_locked(ds);
	pthread_mutex_unlock(&ds->debug_file_lock);

	/* Wrap up */
	pthread_cond_destroy(&ds->log_cond);
	pthread_mutex_destroy(&ds->debug_lock);
	pthread_mutex_destroy(&ds->debug_file_lock);
}

void
debug_shutdown(void)
{
	int i;

	debug_shutdown_instance(&debugInstance);

	for (i = 0; i < DEBUG_SECTION_MAX; i++) {
		if (debug_level_strs[i] != NULL) {
			free(debug_level_strs[i]);
			debug_level_strs[i] = NULL;
		}
	}
	bzero(debug_levels, sizeof(debug_levels));
}
