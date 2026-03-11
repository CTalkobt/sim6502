#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"
#include "io_handler.h"
#include "io_registry.h"

/* Physical (raw) byte read — no MAP translation.
 * Used for far/flat 28-bit access and internally after MAP address translation. */
static inline unsigned char mem_read_phys(memory_t *mem, unsigned int phys) {
	if (phys < 0x10000) {
		uint8_t val;
		if (mem->io_handlers[phys] && mem->io_handlers[phys]->io_read(mem, (uint16_t)phys, &val))
			return val;
		return mem->mem[phys];
	}
	unsigned int page = phys >> FAR_PAGE_SHIFT;
	unsigned int off  = phys & (FAR_PAGE_SIZE - 1);
	if (!mem->far_pages[page])
		return 0;
	return mem->far_pages[page][off];
}

/**
 * Debug/Introspection Read: Returns value without side-effects.
 */
static inline unsigned char mem_peek(memory_t *mem, uint16_t addr) {
    uint8_t val;
    if (mem->io_handlers[addr] && mem->io_handlers[addr]->io_peek(mem, addr, &val))
        return val;
    return mem->mem[addr];
}

/* MEGA65 DMA execution */
#define DMA_MODE_LEGACY   0
#define DMA_MODE_ENHANCED 1

static inline void mem_write_phys(memory_t *mem, unsigned int phys, unsigned char val);

static inline void mem_dma_execute(memory_t *mem, unsigned char val, int mode) {
	unsigned int job_addr = val | (mem->mem[0xD701] << 8) | (mem->mem[0xD702] << 16) | ((mem->mem[0xD704] & 0x0F) << 24);
	unsigned int ptr = job_addr;
	int format = (mode == DMA_MODE_LEGACY) ? 11 : 12;
	unsigned int src_mb = 0, dst_mb = 0;

	unsigned char token = mem_read_phys(mem, ptr);
	if (mode == DMA_MODE_ENHANCED && (token == 0x0A || token == 0x0B)) {
		ptr++; format = (token == 0x0A) ? 11 : 12;
		token = mem_read_phys(mem, ptr++);
		while (token != 0x00 && ptr - job_addr < 256) {
			if (token == 0x80) src_mb = mem_read_phys(mem, ptr++);
			else if (token == 0x81) dst_mb = mem_read_phys(mem, ptr++);
			else if (token & 0x80) ptr++;
			token = mem_read_phys(mem, ptr++);
		}
	}

	unsigned char cmd = mem_read_phys(mem, ptr);
	unsigned int count = mem_read_phys(mem, ptr + 1) | (mem_read_phys(mem, ptr + 2) << 8);
	if (count == 0) count = 65536;

	unsigned int src = mem_read_phys(mem, ptr + 3) | (mem_read_phys(mem, ptr + 4) << 8) | 
	                  ((mem_read_phys(mem, ptr + 5) & 0x0F) << 16) | (src_mb << 20);
	unsigned int dst = mem_read_phys(mem, ptr + 6) | (mem_read_phys(mem, ptr + 7) << 8) | 
	                  ((mem_read_phys(mem, ptr + 8) & 0x0F) << 16) | (dst_mb << 20);

	if ((cmd & 0x03) == 0x00) {
		for (unsigned int i = 0; i < count; i++) mem_write_phys(mem, dst + i, mem_read_phys(mem, src + i));
	} else if ((cmd & 0x03) == 0x03 || (format == 11 && (cmd & 0x03) == 0x02)) {
		unsigned char fill_val = mem_read_phys(mem, ptr + 3);
		for (unsigned int i = 0; i < count; i++) mem_write_phys(mem, dst + i, fill_val);
	}
}

static inline void mem_write_phys(memory_t *mem, unsigned int phys, unsigned char val) {
	if (phys < 0x10000) {
		if (mem->io_handlers[phys] && mem->io_handlers[phys]->io_write(mem, (uint16_t)phys, val))
			return;
		mem->mem[phys] = val;
		return;
	}
	unsigned int page = phys >> FAR_PAGE_SHIFT;
	unsigned int off  = phys & (FAR_PAGE_SIZE - 1);
	if (!mem->far_pages[page]) mem->far_pages[page] = (unsigned char *)calloc(FAR_PAGE_SIZE, 1);
	mem->far_pages[page][off] = val;
}

static inline unsigned char mem_read(memory_t *mem, unsigned short addr) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		return mem_read_phys(mem, phys);
	}
	uint8_t val;
	if (mem->io_handlers[addr] && mem->io_handlers[addr]->io_read(mem, addr, &val))
		return val;
	return mem->mem[addr];
}

static inline void mem_write(memory_t *mem, unsigned short addr, unsigned char val) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->mem_writes < 256) {
		mem->mem_addr[mem->mem_writes] = addr;
		mem->mem_old_val[mem->mem_writes] = mem_read(mem, addr);
		mem->mem_val[mem->mem_writes] = val;
	}
	mem->mem_writes++;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		mem_write_phys(mem, phys, val);
	} else {
		if (mem->io_handlers[addr] && mem->io_handlers[addr]->io_write(mem, addr, val))
			return;
		mem_write_phys(mem, addr, val);
	}
}

static inline unsigned char far_mem_read(memory_t *m, unsigned int addr) {
	return mem_read_phys(m, addr);
}

static inline void far_mem_write(memory_t *m, unsigned int addr, unsigned char val) {
	if (addr < 0x10000) {
		if (m->mem_writes < 256) {
			m->mem_addr[m->mem_writes] = (unsigned short)addr;
			m->mem_old_val[m->mem_writes] = far_mem_read(m, addr);
			m->mem_val[m->mem_writes] = val;
		}
		m->mem_writes++;
	}
	mem_write_phys(m, addr, val);
}

#endif
