/**
 * @file rootkiticide.h
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief general declarations for rootkiticide
 */

#pragma once

#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/net.h>

#define PROCNAME "rootkiticide"	/* need to be unique per each check */

/* scheduler_hook.c */
int __must_check scheduler_hook_init(void);
void scheduler_hook_cleanup(void);

/* fd_hook.c */
int __must_check fd_hook_init(void);
void fd_hook_cleanup(void);

/* hw_breakpoint.c */
struct perf_event * __percpu * __must_check hbp_on_exec(
	const char *const funcname,
	const perf_overflow_handler_t handler);
void hbp_clear(struct perf_event * __percpu *hbp);

int __must_check is_kernel_address_valid(ulong addr);

/* proc.c */
int __must_check proc_init(void);
void proc_cleanup(void);
int __must_check log_socket(const struct sockaddr_storage *const saddr);
int __must_check log_process(void);
int __must_check log_file(const char *const filename);
