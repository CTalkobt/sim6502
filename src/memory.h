#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>

#define FAR_PAGE_SHIFT  12
#define FAR_PAGE_SIZE   (1 << FAR_PAGE_SHIFT)		/* 4096 bytes per page */
#define FAR_NUM_PAGES   (0x10000000 >> FAR_PAGE_SHIFT)	/* 65536 pages for 28-bit space */

typedef struct {
	unsigned char mem[0x10000];
	int mem_writes;
	unsigned short mem_addr[256];
	unsigned char mem_val[256];
	unsigned char *far_pages[FAR_NUM_PAGES];	/* sparse 28-bit page table */
} memory_t;

static inline void mem_write(memory_t *mem, unsigned short addr, unsigned char val) {
	mem->mem[addr] = val;
	if (mem->mem_writes < 256) {
		mem->mem_addr[mem->mem_writes] = addr;
		mem->mem_val[mem->mem_writes] = val;
	}
	mem->mem_writes++;
}

static inline unsigned char mem_read(memory_t *mem, unsigned short addr) {
	return mem->mem[addr];
}

/* Read a byte from the full 28-bit address space.
 * Addresses below 0x10000 map directly to the 64 KB mem[] array. */
static inline unsigned char far_mem_read(memory_t *m, unsigned int addr) {
	if (addr < 0x10000)
		return m->mem[addr];
	unsigned int page = addr >> FAR_PAGE_SHIFT;
	unsigned int off  = addr & (FAR_PAGE_SIZE - 1);
	if (!m->far_pages[page])
		return 0;
	return m->far_pages[page][off];
}

/* Write a byte to the full 28-bit address space.
 * Addresses below 0x10000 go through mem_write() so the write-log stays coherent.
 * Upper pages are lazily allocated in 4 KB chunks. */
static inline void far_mem_write(memory_t *m, unsigned int addr, unsigned char val) {
	if (addr < 0x10000) {
		mem_write(m, (unsigned short)addr, val);
		return;
	}
	unsigned int page = addr >> FAR_PAGE_SHIFT;
	unsigned int off  = addr & (FAR_PAGE_SIZE - 1);
	if (!m->far_pages[page])
		m->far_pages[page] = (unsigned char *)calloc(FAR_PAGE_SIZE, 1);
	m->far_pages[page][off] = val;
}

#endif
