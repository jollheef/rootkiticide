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
#include <linux/hw_breakpoint.h>

static struct perf_event * __percpu *try_to_wake_up_hbp;

static void try_to_wake_up_handler(struct perf_event *bp,
				   struct perf_sample_data *data,
				   struct pt_regs *regs)
{
	printk("rkcd: pid=%d, tgid=%d, comm=%s\n",
	       current->pid, current->tgid, current->comm);
}

int scheduler_hook_init(void)
{
	/* Set hardware breakpoint on try_to_wake_up */
	struct perf_event_attr attr;
	hw_breakpoint_init(&attr);

	attr.bp_addr = kallsyms_lookup_name("try_to_wake_up");
	if (!attr.bp_addr)
		return -EINVAL;
	attr.bp_len = HW_BREAKPOINT_LEN_8;
	attr.bp_type = HW_BREAKPOINT_X;

	try_to_wake_up_hbp = register_wide_hw_breakpoint(
		&attr,
		try_to_wake_up_handler,
		NULL);

	if (IS_ERR(try_to_wake_up_hbp))
		return PTR_ERR(try_to_wake_up_hbp);

	return 0;
}

void scheduler_hook_cleanup(void)
{
	unregister_wide_hw_breakpoint(try_to_wake_up_hbp);
}
