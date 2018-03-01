#ifndef	__DEBUG_INTERNAL_H__
#define	__DEBUG_INTERNAL_H__

/*
 * These are internal structures to the debug framework.
 */
struct debug_entry {
	TAILQ_ENTRY(debug_entry) e;
	struct timeval tv;
	debug_section_t debug_section;
	debug_mask_t debug_mask;
	char buf[512];
};

struct debug_instance {
	TAILQ_HEAD(, debug_entry) list;
	int nitems;

	pthread_t log_thread;
	pthread_cond_t log_cond;
	pthread_mutex_t debug_lock;
	pthread_mutex_t debug_file_lock;
	int debug_thr_do_exit;
	int debug_queue_limit;

	/* Syslog configuration */
	int debug_syslog_facility;
	int debug_syslog_logopt;
	char *debug_syslog_ident;
	int debug_syslog_enable;

	/* File logging configuration */
	FILE *debug_file;
	char *debug_filename;
};

#endif
