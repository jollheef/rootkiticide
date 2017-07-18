#include "ringbuf.h"
#include "ringbuf_internal.h"

void ringbuf_init(struct ringbuf * const rb)
{
	ulong i;

	rb->blocks = kcalloc(sizeof(*rb->blocks), RB_NUM_BLOCKS + 1, GFP_KERNEL);
	rb->block_map = kcalloc(sizeof(*rb->block_map), RB_NUM_BLOCKS, GFP_KERNEL);
	for (i = 0; i < RB_NUM_BLOCKS + 1; i++) {
		rb->blocks[i].pages = alloc_pages(GFP_KERNEL,
						get_count_order(RB_BLOCK_PAGES));
		atomic_set(&rb->blocks[i].reserved, 0);
		atomic_set(&rb->blocks[i].occupied, 0);
		rb->blocks[i].ptr = page_address(&rb->blocks[i].pages[0]);
	}

	for (i = 0; i < RB_NUM_BLOCKS; i++) {
		atomic_set(&rb->block_map[i], i);
	}
	atomic_set(&rb->read_map, RB_NUM_BLOCKS);
}

void ringbuf_free(struct ringbuf * const rb)
{
	ulong i;

	for (i = 0; i < RB_NUM_BLOCKS + 1; i++)
		while (atomic_read(&rb->blocks[i].reserved)) {} //fixme

	for (i = 0; i < RB_NUM_BLOCKS + 1; i++)
		__free_pages(rb->blocks[i].pages, get_count_order(RB_BLOCK_PAGES));
	kfree(rb->block_map);
	kfree(rb->blocks);
}


void *ringbuf_reserve(struct ringbuf * const rb, struct commit_s *commit)
{
	void *addr;
	ulong offset;
	ulong blocknum;
	long extra_bytes;
	struct block *block;
	size_t size = commit->size;
	struct entry_header *header;

retry:
	offset = atomic_add_return(size + RB_HEADER_SIZE, &rb->tail)
			- size - RB_HEADER_SIZE;
	blocknum = offset_to_blocknum(rb, offset);
	block = block_acquire(rb, blocknum);
	addr = block->ptr + offset_in_block(rb, offset);
	extra_bytes = (offset_in_block(rb, offset) + size + RB_HEADER_SIZE) - RB_BLOCK_SIZE;
	if (unlikely(extra_bytes > 0)) {
		/*
		 * Boundary wrap
		 */
		ulong rem_bytes = size + RB_HEADER_SIZE - extra_bytes;
		atomic_add(rem_bytes, &block->reserved);
		write_skip_header(addr, rem_bytes, rem_bytes);
		check_overwrite(rb, block);
		atomic_add(rem_bytes, &block->occupied);
		atomic_sub(rem_bytes, &block->reserved);
		block_release(rb, blocknum);

		offset += rem_bytes;
		blocknum = offset_to_blocknum(rb, offset);
		block = block_acquire(rb, blocknum);
		addr = block->ptr + offset_in_block(rb, offset);
		atomic_add(extra_bytes, &block->reserved);
		write_skip_header(addr, extra_bytes, extra_bytes);
		check_overwrite(rb, block);
		atomic_add(extra_bytes, &block->occupied);
		atomic_sub(extra_bytes, &block->reserved);

		goto retry;
	}

	atomic_add(size + RB_HEADER_SIZE, &block->reserved);
	if (extra_bytes == 0)
		block_release(rb, blocknum);

	header = addr;
	header->skip_header = false;
	header->short_header = false;
	header->long_size = size;

	commit->block = block;
	return addr + RB_HEADER_SIZE;
}

void ringbuf_commit(struct ringbuf * const rb, const struct commit_s *commit)
{
	check_overwrite(rb, commit->block);
	atomic_add(commit->size + RB_HEADER_SIZE, &commit->block->occupied);
	smp_wmb();
	atomic_sub(commit->size + RB_HEADER_SIZE, &commit->block->reserved);
}

static int __must_check switch_readblock(struct ringbuf * const rb)
{
	ulong blocknum, switchwith_id, readblock_id;

	blocknum = offset_to_blocknum(rb, atomic_read(&rb->head));
	readblock_id = atomic_read(&rb->read_map) & RB_BLOCKID_MASK;
	switchwith_id = atomic_read(&rb->block_map[blocknum]) & RB_BLOCKID_MASK;
	/* check if used by the writer and switch atomically */
	if (atomic_cmpxchg(&rb->block_map[blocknum], switchwith_id, readblock_id)
			!= switchwith_id)
		return 1; // fixme: proper error
	atomic_set(&rb->read_map, switchwith_id);
	return 0;
}

void * __must_check ringbuf_read(struct ringbuf * const rb)
{
	void *addr;
	struct entry_header *header;
	size_t size;
	ulong occupied;
	struct block *readblock = readblock_get(rb);

entry:
	if (!(occupied = atomic_read(&readblock->occupied))) {
		if (switch_readblock(rb))
			return NULL;
		readblock = readblock_get(rb);
		while (atomic_read(&readblock->reserved)) {
			// fixme: need wait queue
		}
		smp_rmb();
		occupied = atomic_read(&readblock->occupied);
	}

	header = addr = readblock->ptr + RB_BLOCK_SIZE - occupied;

	if (unlikely(header->skip_header)) {
		if (header->short_header) {
			size = header->short_size;
			atomic_add(size + 1, &rb->head);
			atomic_sub(size + 1, &readblock->occupied);
		} else {
			size = header->long_size;
			atomic_add(size + RB_HEADER_SIZE, &rb->head);
			atomic_sub(size + RB_HEADER_SIZE, &readblock->occupied);
		}
		goto entry;
	}

	size = header->long_size;
	atomic_add(size + RB_HEADER_SIZE, &rb->head);
	atomic_sub(size + RB_HEADER_SIZE, &readblock->occupied);
	return addr + RB_HEADER_SIZE;
}
