#pragma once

#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/highmem.h>

struct block {
	void *ptr;		/* virtual memory address */
	atomic_t occupied;	/* bytes committed */
	struct page *pages;	/* underlying pages */
};

struct ringbuf {
	atomic_t *block_map;	/*
				 * Remaps blocks' indices.
				 * Maps 0..NUM_BLOCKS-1 to offsets
				 * in 'struct block *blocks'.
				 * It is used to allow swapping blocks
				 * with read_map for reading.
				 * Max 23 bits entries. Bit 24 is reserved
				 * for exclusive access.
				 */

	atomic_t read_map;	/*
				 * Index to block
				 * in 'struct block *blocks'
				 * currently swapped out for reading.
				 */

	atomic_t head; /* next byte to read */
	atomic_t tail; /* next byte to write */

	struct block *blocks; /* array of storage blocks */
};

struct commit_s {
	ulong blocknum;
	size_t size;
};

void ringbuf_init(struct ringbuf * const rb);
void ringbuf_free(struct ringbuf * const rb);
void *ringbuf_reserve(struct ringbuf * const rb, struct commit_s *commit);
void ringbuf_commit(struct ringbuf * const rb, const struct commit_s *commit);
void * __must_check ringbuf_read(struct ringbuf * const rb);
