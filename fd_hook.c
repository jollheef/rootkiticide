/**
 * @file fd_hook.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief file descriptor hooks for revealing hidden operations with fds
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/perf_event.h>
#include <linux/namei.h>
#include <linux/fdtable.h>
#include <linux/net.h>
#include <linux/delay.h>
#include <asm/pgtable.h>

#include "rootkiticide.h"

static struct perf_event * __percpu *vfs_write_hbp;
static struct perf_event * __percpu *vfs_writev_hbp;

static int __must_check dump_socket(struct socket *sock)
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

	return log_socket(&saddr);
}

static int __must_check dump_file(struct file *file)
{
	char buf[PATH_MAX] = { 0 };
	char *filename = d_path(&file->f_path, buf, sizeof(buf));
	if (IS_ERR_OR_NULL(filename))
		return PTR_ERR(filename);

	return log_file(filename);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
/* EXPORT_SYMBOL after 406a3c638ce8b17d9704052c07955490f732c2b8 */
static struct file_operations *socket_op;
static struct socket *sock_from_file(struct file *file, int *err)
{
	if (file->f_op == socket_op)
		return file->private_data;	/* set in sock_map_fd */

	*err = -ENOTSOCK;
	return NULL;
}
#endif

static int __must_check dump_all_fds(const void *v, struct file *file, uint fd)
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
/* helper introduced in c3c073f808b22dfae15ef8412b6f7b998644139a */
int iterate_fd(struct files_struct *files, unsigned n,
		int (*f)(const void *, struct file *, unsigned),
		const void *p)
{
	struct fdtable *fdt;
	int res = 0;
	if (!files)
		return 0;
	spin_lock(&files->file_lock);
	for (fdt = files_fdtable(files); n < fdt->max_fds; n++) {
		struct file *file;
		file = rcu_dereference_check_fdtable(files, fdt->fd[n]);
		if (!file)
			continue;
		res = f(p, file, n);
		if (res)
			break;
	}
	spin_unlock(&files->file_lock);
	return res;
}
#endif

static atomic_t x_fd_handler_usage = ATOMIC_INIT(0);
static void x_fd_handler(struct perf_event *bp,
			 struct perf_sample_data *data,
			 struct pt_regs *regs)
{
	atomic_inc(&x_fd_handler_usage);
	iterate_fd(current->files, 0, dump_all_fds, NULL);
	atomic_dec(&x_fd_handler_usage);
}

int __must_check fd_hook_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
/* EXPORT_SYMBOL after 406a3c638ce8b17d9704052c07955490f732c2b8 */
	socket_op = (typeof(socket_op))kallsyms_lookup_name("socket_file_ops");
	if (!socket_op || !is_kernel_address_valid((ulong)socket_op))
		return -EINVAL;
#endif
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
	while (atomic_read(&x_fd_handler_usage))
		msleep_interruptible(100);

	hbp_clear(vfs_write_hbp);
	hbp_clear(vfs_writev_hbp);
}
