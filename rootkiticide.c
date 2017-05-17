/**
 * @file rootkiticide.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief kernel module for rootkit revealing
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

#include "rootkiticide.h"

static int rootkiticide_init(void)
{
	ulong ret = scheduler_hook_init();
	if (IS_ERR_VALUE(ret))
		return ret;

	ret = fd_hook_init();
	if (IS_ERR_VALUE(ret)) {
		scheduler_hook_cleanup();
		return ret;
	}

	ret = proc_init();
	if (IS_ERR_VALUE(ret)) {
		scheduler_hook_cleanup();
		fd_hook_cleanup();
		return ret;
	}

	printk("rkcd: init success\n");
	return 0;		/* success */
}
module_init(rootkiticide_init);

static void rootkiticide_exit(void)
{
	scheduler_hook_cleanup();
	fd_hook_cleanup();
	proc_cleanup();
	printk("rkcd: cleanup\n");
}
module_exit(rootkiticide_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mikhail Klementyev <jollheef@riseup.net>");
