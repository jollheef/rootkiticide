/**
 * @file scheduler_hook.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief scheduler hooks for revealing hidden process
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/perf_event.h>

#include "rootkiticide.h"

static struct perf_event * __percpu *try_to_wake_up_hbp;

static void try_to_wake_up_handler(struct perf_event *bp,
				   struct perf_sample_data *data,
				   struct pt_regs *regs)
{
	printk("rkcd:sched pid=%d, tgid=%d, comm=%s\n",
	current->pid, current->tgid, current->comm);
}

int scheduler_hook_init(void)
{
	/* Set hardware breakpoint on try_to_wake_up */
	try_to_wake_up_hbp = hbp_on_exec("try_to_wake_up",
					 try_to_wake_up_handler);
	if (IS_ERR(try_to_wake_up_hbp))
		return PTR_ERR(try_to_wake_up_hbp);

	return 0;
}

void scheduler_hook_cleanup(void)
{
	hbp_clear(try_to_wake_up_hbp);
}
