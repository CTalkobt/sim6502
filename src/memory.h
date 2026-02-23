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
	unsigned int map_offset[8];		/* MAP: per-8KB-block physical offset added to virtual addr; 0 = passthrough */
} memory_t;

/* Physical (raw) byte read — no MAP translation.
 * Used for far/flat 28-bit access and internally after MAP address translation. */
static inline unsigned char mem_read_phys(memory_t *mem, unsigned int phys) {
	if (phys < 0x10000)
		return mem->mem[phys];
	unsigned int page = phys >> FAR_PAGE_SHIFT;
	unsigned int off  = phys & (FAR_PAGE_SIZE - 1);
	if (!mem->far_pages[page])
		return 0;
	return mem->far_pages[page][off];
}

/* Physical (raw) byte write — no MAP translation. */
static inline void mem_write_phys(memory_t *mem, unsigned int phys, unsigned char val) {
	if (phys < 0x10000) {
		mem->mem[phys] = val;
		return;
	}
	unsigned int page = phys >> FAR_PAGE_SHIFT;
	unsigned int off  = phys & (FAR_PAGE_SIZE - 1);
	if (!mem->far_pages[page])
		mem->far_pages[page] = (unsigned char *)calloc(FAR_PAGE_SIZE, 1);
	mem->far_pages[page][off] = val;
}

/* Virtual memory read — applies MAP offset for the 8KB block if non-zero.
 * Physical address wraps at 20 bits per the 45GS02/C65 specification. */
static inline unsigned char mem_read(memory_t *mem, unsigned short addr) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		return mem_read_phys(mem, phys);
	}
	return mem->mem[addr];
}

/* Virtual memory write — applies MAP offset for the 8KB block if non-zero.
 * Always logs the virtual address in the write trace. */
static inline void mem_write(memory_t *mem, unsigned short addr, unsigned char val) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		mem_write_phys(mem, phys, val);
	} else {
		mem->mem[addr] = val;
	}
	if (mem->mem_writes < 256) {
		mem->mem_addr[mem->mem_writes] = addr;
		mem->mem_val[mem->mem_writes] = val;
	}
	mem->mem_writes++;
}

/* Read from the full 28-bit address space (flat/EOM-prefix mode).
 * Physical addresses — MAP is NOT applied. */
static inline unsigned char far_mem_read(memory_t *m, unsigned int addr) {
	return mem_read_phys(m, addr);
}

/* Write to the full 28-bit address space (flat/EOM-prefix mode).
 * Physical addresses — MAP is NOT applied.
 * Maintains the write log for addresses in the 16-bit range. */
static inline void far_mem_write(memory_t *m, unsigned int addr, unsigned char val) {
	mem_write_phys(m, addr, val);
	if (addr < 0x10000) {
		if (m->mem_writes < 256) {
			m->mem_addr[m->mem_writes] = (unsigned short)addr;
			m->mem_val[m->mem_writes] = val;
		}
		m->mem_writes++;
	}
}

#endif
