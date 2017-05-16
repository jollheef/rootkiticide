/**
 * @file rootkiticide.h
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief general declarations for rootkiticide
 */

#pragma once

int scheduler_hook_init(void);
void scheduler_hook_cleanup(void);

int fd_hook_init(void);
void fd_hook_cleanup(void);

__attribute__((warn_unused_result)) struct perf_event * __percpu *
hbp_on_exec(const char *const funcname,
	    const perf_overflow_handler_t handler);
void hbp_clear(struct perf_event * __percpu *hbp);

int is_kernel_address_valid(ulong addr);
