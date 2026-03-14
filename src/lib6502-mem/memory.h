#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "memory_types.h"
#include "io_handler.h"
#include "io_registry.h"

/* Physical (raw) byte read — no MAP translation.
 * Used for far/flat 28-bit access and internally after MAP address translation. */
unsigned char mem_read_phys(memory_t *mem, unsigned int phys);

/**
 * Debug/Introspection Read: Returns value without side-effects.
 */
unsigned char mem_peek(memory_t *mem, uint16_t addr);

/**
 * Physical (raw) byte write — no MAP translation.
 */
void mem_write_phys(memory_t *mem, unsigned int phys, unsigned char val);

/**
 * Logical 16-bit address read with MAP translation.
 */
unsigned char mem_read(memory_t *mem, unsigned short addr);

/**
 * Logical 16-bit address write with MAP translation.
 */
void mem_write(memory_t *mem, unsigned short addr, unsigned char val);

/**
 * 28-bit address read.
 */
unsigned char far_mem_read(memory_t *m, unsigned int addr);

/**
 * 28-bit address write.
 */
void far_mem_write(memory_t *m, unsigned int addr, unsigned char val);

/**
 * Free all allocated far memory pages.
 */
void mem_free_far_pages(memory_t *mem);

#endif // MEMORY_H
