/**
 * @file rootkiticide.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief kernel module for rootkit revealing
 */

#include <linux/module.h>
#include <linux/kernel.h>

static int rootkiticide_init(void)
{
	return 0;		/* success */
}
module_init(rootkiticide_init);

static void rootkiticide_exit(void)
{
}
module_exit(rootkiticide_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mikhail Klementyev <jollheef@riseup.net>");
