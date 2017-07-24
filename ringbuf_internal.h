/**
 * @file ringbuf_internal.h
 * @author Aleksandr Trukhin <alxtry@gmail.com>
 * @date July 2017
 * @brief simple ringbuffer
 */

#include "ringbuf.h"


#define RB_NUM_BLOCKS 16	/* Must be a power of 2 */
#define RB_BLOCK_PAGES 32	/* Must be a power of 2 */
#define RB_BLOCK_SIZE (RB_BLOCK_PAGES << PAGE_SHIFT)

#define RB_HEADER_SIZE sizeof(struct entry_header)

#define RB_BLOCKRESERVE_BIT (1 << 23)
#define RB_BLOCKID_MASK (RB_BLOCKRESERVE_BIT - 1)


/* header for data entries in the block */
struct entry_header {
	struct {
		bool skip_header : 1;	/* block is empty skip block */
		bool short_header : 1;	/* block is 1 byte long (for short skip blocks) */
		size_t short_size : 6;	/* size of the data (excluding header) */
	} __attribute__((packed));
	size_t long_size : 32;		/* size of the data (excluding header) */
} __attribute__((packed));


static inline
struct block *readblock_get(struct ringbuf * const rb)
{
	return &rb->blocks[atomic_read(&rb->read_map)];
}

static inline
ulong offset_to_blocknum(const ulong offset)
{
	return (offset >> get_count_order(RB_BLOCK_SIZE)) % RB_NUM_BLOCKS;
}

static inline
struct block *block_acquire(struct ringbuf * const rb, const ulong blocknum)
{
	ulong blockmap;

	/* try to set the reserved bit */
	do {
		// max 2/3 times ?
		blockmap = atomic_read(&rb->block_map[blocknum]);
	} while (atomic_cmpxchg(&rb->block_map[blocknum], blockmap,
				blockmap | RB_BLOCKRESERVE_BIT) != blockmap);
	return &rb->blocks[blockmap & RB_BLOCKID_MASK];
}

static inline
bool block_acquired(struct ringbuf * const rb, const ulong blocknum)
{
	ulong blockmap = atomic_read(&rb->block_map[blocknum]);
	return (blockmap & RB_BLOCKRESERVE_BIT) ? true : false;
}

static inline
void block_release(struct ringbuf * const rb, const ulong blocknum)
{
	/* need barrier before the call */
	ulong blockmap = atomic_read(&rb->block_map[blocknum]);
	atomic_set(&rb->block_map[blocknum], blockmap & RB_BLOCKID_MASK);
}

static inline
ulong offset_in_block(const ulong offset)
{
	return offset & (RB_BLOCK_SIZE - 1);
}

static inline
void write_skip_header(void *addr, size_t skip, size_t b_avail)
{
	struct entry_header *header = addr;
	header->skip_header = true;
	if (unlikely(b_avail < RB_HEADER_SIZE)) { // short header
		header->short_header = true;
		header->short_size = skip - 1;
	} else { // long header
		header->short_header = false;
		header->long_size = skip - RB_HEADER_SIZE;
	}
}

static inline
void check_overwrite(struct ringbuf * const rb, struct block * const block)
{
	ulong old_head;

	/*
	 * Overwrite block if fully written
	 * push reader by block (only once if there are concurrent writes)
	 */
	if (atomic_cmpxchg(&block->occupied, RB_BLOCK_SIZE, 0) == RB_BLOCK_SIZE) {
		old_head = atomic_read(&rb->head);
		atomic_cmpxchg(&rb->head, old_head, old_head + RB_BLOCK_SIZE);
	}
}

static inline
void finalize_commit(struct ringbuf * const rb, struct block * const block,
		const ulong blocknum, const ulong bytes)
{
	check_overwrite(rb, block);
	if (atomic_add_return(bytes, &block->occupied) == RB_BLOCK_SIZE)
		block_release(rb, blocknum);
}

static inline
void boundary_wrap(struct ringbuf * const rb, const ulong blocknum,
		const ulong offset, const size_t size, const ulong overflow_bytes)
{
	struct block *block = block_acquire(rb, blocknum);
	void *addr = block->ptr + offset_in_block(offset);
	ulong prev_bytes = size + RB_HEADER_SIZE - overflow_bytes;

	write_skip_header(addr, prev_bytes, prev_bytes);
	finalize_commit(rb, block, blocknum, prev_bytes);

	block = block_acquire(rb, (blocknum + 1) % RB_NUM_BLOCKS);
	addr = block->ptr;
	write_skip_header(addr, overflow_bytes, overflow_bytes);
	finalize_commit(rb, block, (blocknum + 1) % RB_NUM_BLOCKS, overflow_bytes);
}
