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
	unsigned char mem_val[256];     /* value AFTER write  */
	unsigned char mem_old_val[256]; /* value BEFORE write */
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

/* MEGA65 math coprocessor — recompute results whenever an input register is
 * written.  Registers are combinatorial on real hardware; we mirror that here.
 *
 *   $D768/$D769  Divider dividend (A)       $D76A/$D76B  Divider divisor (B)
 *   $D770/$D771  Multiplier input A         $D772/$D773  Multiplier input B
 *   $D778-$D77B  Shared 32-bit result (LSB first)
 *
 * MUL write  → result = MUL_A × MUL_B  (32-bit)
 * DIV write  → result bytes 0-1 = quotient, bytes 2-3 = remainder
 */
static inline void mem_math_update(memory_t *mem, unsigned short addr) {
	if (addr >= 0xD770 && addr <= 0xD773) {
		/* Multiply */
		unsigned int a = mem->mem[0xD770] | ((unsigned int)mem->mem[0xD771] << 8);
		unsigned int b = mem->mem[0xD772] | ((unsigned int)mem->mem[0xD773] << 8);
		unsigned long long p = (unsigned long long)a * b;
		mem->mem[0xD778] = (unsigned char)(p);
		mem->mem[0xD779] = (unsigned char)(p >> 8);
		mem->mem[0xD77A] = (unsigned char)(p >> 16);
		mem->mem[0xD77B] = (unsigned char)(p >> 24);
	} else if (addr >= 0xD768 && addr <= 0xD76B) {
		/* Divide */
		unsigned int a = mem->mem[0xD768] | ((unsigned int)mem->mem[0xD769] << 8);
		unsigned int b = mem->mem[0xD76A] | ((unsigned int)mem->mem[0xD76B] << 8);
		unsigned int q = 0, r = 0;
		if (b) { q = a / b; r = a % b; }
		mem->mem[0xD778] = (unsigned char)(q);
		mem->mem[0xD779] = (unsigned char)(q >> 8);
		mem->mem[0xD77A] = (unsigned char)(r);
		mem->mem[0xD77B] = (unsigned char)(r >> 8);
	}
}

/* Physical (raw) byte write — no MAP translation. */
static inline void mem_write_phys(memory_t *mem, unsigned int phys, unsigned char val) {
	if (phys < 0x10000) {
		mem->mem[phys] = val;
		mem_math_update(mem, (unsigned short)phys);
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
 * Logs the virtual address and both the old and new values before writing. */
static inline void mem_write(memory_t *mem, unsigned short addr, unsigned char val) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->mem_writes < 256) {
		mem->mem_addr[mem->mem_writes]    = addr;
		mem->mem_old_val[mem->mem_writes] = mem_read(mem, addr); /* capture old value */
		mem->mem_val[mem->mem_writes]     = val;
	}
	mem->mem_writes++;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		mem_write_phys(mem, phys, val);
	} else {
		mem->mem[addr] = val;
		mem_math_update(mem, addr);
	}
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
	if (addr < 0x10000) {
		if (m->mem_writes < 256) {
			m->mem_addr[m->mem_writes]    = (unsigned short)addr;
			m->mem_old_val[m->mem_writes] = far_mem_read(m, addr); /* capture old value */
			m->mem_val[m->mem_writes]     = val;
		}
		m->mem_writes++;
	}
	mem_write_phys(m, addr, val);
}

#endif
