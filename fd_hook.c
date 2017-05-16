/**
 * @file fd_hook.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief file descriptor hooks for revealing hidden operations with fds
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/perf_event.h>
#include <linux/namei.h>
#include <linux/fdtable.h>
#include <asm/pgtable.h>
#include <linux/net.h>

#include "rootkiticide.h"

static struct perf_event * __percpu *vfs_write_hbp;
static struct perf_event * __percpu *vfs_writev_hbp;

__attribute__((warn_unused_result))
static int dump_socket(struct socket *sock)
{
	ulong ret;
	struct sockaddr_storage saddr;
	int buflen;
	ret = sock->ops->getname(sock, (struct sockaddr *)&saddr, &buflen, 1);
	if (IS_ERR_VALUE(ret))
		return ret;

	sa_family_t family = ((struct sockaddr *)&saddr)->sa_family;
	if (family != AF_INET && family != AF_INET6)
		return -EINVAL;

	printk("rkcd:netop pid=%d tgid=%d comm=%s sock=%pISpc\n",
	       current->pid, current->tgid, current->comm,
	       &saddr);

	return 0;
}

__attribute__((warn_unused_result))
static int dump_file(struct file *file)
{
	char buf[PATH_MAX] = { 0 };
	char *filename = dentry_path_raw(file->f_path.dentry,
					 buf, sizeof(buf));
	if (IS_ERR_OR_NULL(filename))
		return PTR_ERR(filename);

	printk("rkcd:vfsop pid=%d tgid=%d comm=%s file=%s\n",
	       current->pid, current->tgid, current->comm,
	       filename);

	return 0;
}

static int dump_all_fds(const void *v, struct file *file, uint fd)
{
	int err;

	if (!is_kernel_address_valid((ulong)file))
		return -EINVAL;

	struct socket *sock = sock_from_file(file, &err);
	if (sock)
		return dump_socket(sock);
	else if (err == -ENOTSOCK)
		return dump_file(file);

	BUG();
	return 0;
}

static void x_fd_handler(struct perf_event *bp,
			 struct perf_sample_data *data,
			 struct pt_regs *regs)
{
	ulong ret = iterate_fd(current->files, 0, dump_all_fds, NULL);
	if (IS_ERR_VALUE(ret))
		return;
}

int fd_hook_init(void)
{
	/* Set hardware breakpoint on vfs functions */
	vfs_write_hbp = hbp_on_exec("__vfs_write", x_fd_handler);
	if (IS_ERR(vfs_write_hbp))
		return PTR_ERR(vfs_write_hbp);

	vfs_writev_hbp = hbp_on_exec("vfs_writev", x_fd_handler);
	if (IS_ERR(vfs_writev_hbp))
		return PTR_ERR(vfs_writev_hbp);

	return 0;
}

void fd_hook_cleanup(void)
{
	hbp_clear(vfs_write_hbp);
	hbp_clear(vfs_writev_hbp);
}
