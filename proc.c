/**
 * @file proc.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief procfs logging device
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/net.h>

#include "rootkiticide.h"
#include "ringbuf.h"

spinlock_t log_queue_lock;
LIST_HEAD(log_queue);
atomic_t counter = ATOMIC_INIT(0);

struct ringbuf rbuf = {
	.head = ATOMIC_INIT(0),
	.tail = ATOMIC_INIT(0)
};

static pid_t proc_reader = 0;

enum log_type {
	LOG_SOCKET,
	LOG_FILE,
	LOG_PROCESS
};

struct log_entry {
	struct list_head list;
	ulong id;
	enum log_type log_type;
	struct {
		/* must be filled for all */
		pid_t pid;
		pid_t tgid;
		char comm[TASK_COMM_LEN];
	} common;
	struct {
		/* filled only for LOG_SOCKET */
		struct sockaddr_storage saddr;
	} socket;
	struct {
		/* filled only for LOG_FILE */
		char filename[PATH_MAX + 1];
	} file;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
/* introduced in 6d7581e62f8be462440d7b22c6361f7c9fa4902b */
#define list_first_entry_or_null(ptr, type, member) \
	(!list_empty(ptr) ? list_first_entry(ptr, type, member) : NULL)
#endif

static void *proc_seq_start(struct seq_file *s, loff_t *pos)
{
	return ringbuf_read(&rbuf);
}

static int proc_seq_show(struct seq_file *s, void *v)
{
	struct log_entry *e = v;

	seq_printf(s, "{ ");
	/* TODO escape comm */
	seq_printf(s, "\"id\": %lu, \"pid\": %d, \"tgid\": %d, \"comm\": \"%s\"",
		   e->id, e->common.pid, e->common.tgid, e->common.comm);
	switch (e->log_type) {
	case LOG_PROCESS:
		/* current no additional record info */
		break;
	case LOG_FILE:
		/* TODO escape filename */
		seq_printf(s, ", \"filename\": \"%s\"", e->file.filename);
		break;
	case LOG_SOCKET:
		seq_printf(s, ", \"saddr\": \"%pISpc\"", &e->socket.saddr);
		break;
	}
	seq_printf(s, " }\n");

	return 0;
}

static void *proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return ringbuf_read(&rbuf);
}

static void proc_seq_stop(struct seq_file *s, void *v)
{
}

static const struct seq_operations proc_seq_ops = {
	.start = proc_seq_start,
	.next  = proc_seq_next,
	.stop  = proc_seq_stop,
	.show  = proc_seq_show
};

static int proc_open(struct inode *inode, struct  file *file)
{
	proc_reader = current->real_parent->pid;
	return seq_open(file, &proc_seq_ops);
}

static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __must_check is_reader_or_child(void)
{
	struct task_struct *t, *prev;
	t = current;
	do {
		if (t->pid == proc_reader)
			return true;

		prev = t;
		t = t->real_parent;
	} while (prev->pid != 0);

	return false;
}

static int __must_check log_common(struct log_entry *entry,
				   const struct commit_s *commit)
{
	/*
	if (proc_reader && is_reader_or_child()) {
		kfree(entry);
		return 0;
	}
	*/

	/* fill common log record info */
	entry->common.pid = current->pid;
	entry->common.tgid = current->tgid;
	memcpy(&entry->common.comm, current->comm, sizeof(entry->common.comm));

	entry->id = atomic_read(&counter);
	atomic_inc(&counter);

	ringbuf_commit(&rbuf, commit);
	return 0;
}

int __must_check log_socket(const struct sockaddr_storage *const saddr)
{
	struct commit_s commit = { .size = sizeof(struct log_entry) };
	struct log_entry *entry = ringbuf_reserve(&rbuf, &commit);
	if (!entry)
		return -EFAULT;

	entry->log_type = LOG_SOCKET;
	memcpy(&entry->socket.saddr, saddr, sizeof(entry->socket.saddr));
	return log_common(entry, &commit);
}

int __must_check log_process(void)
{
	struct commit_s commit = { .size = sizeof(struct log_entry) };
	struct log_entry *entry = ringbuf_reserve(&rbuf, &commit);
	if (!entry)
		return -EFAULT;

	entry->log_type = LOG_PROCESS;
	/* current no additional record info */
	return log_common(entry, &commit);
}

int __must_check log_file(const char *const filename)
{
	struct commit_s commit = { .size = sizeof(struct log_entry) };
	struct log_entry *entry = ringbuf_reserve(&rbuf, &commit);
	if (!entry)
		return -EFAULT;

	entry->log_type = LOG_FILE;
	strncpy(entry->file.filename, filename, PATH_MAX);
	return log_common(entry, &commit);
}


int __must_check proc_init(void)
{
	ringbuf_init(&rbuf);

	struct proc_dir_entry *de = proc_create(PROCNAME, 0, NULL, &proc_fops);
	if (IS_ERR(de))
		return PTR_ERR(de);

	return 0;
}

void proc_cleanup(void)
{
	remove_proc_entry(PROCNAME, NULL);

	ringbuf_free(&rbuf);
}
