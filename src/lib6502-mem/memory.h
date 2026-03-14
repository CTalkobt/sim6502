#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "memory_types.h"
#include "io_handler.h"
#include "io_registry.h"

/* Physical (raw) byte read — no MAP translation.
 * Used for far/flat 28-bit access and internally after MAP address translation. */
unsigned char mem_read_phys(memory_t *mem, unsigned int phys);
unsigned char mem_read_phys(const memory_t *mem, unsigned int phys);

/**
 * Debug/Introspection Read: Returns value without side-effects.
 */
unsigned char mem_peek(memory_t *mem, uint16_t addr);
unsigned char mem_peek(const memory_t *mem, uint16_t addr);

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

/* --------------------------------------------------------------------------
 * Overlay management
 * -------------------------------------------------------------------------- */

/* Add an overlay. Returns the overlay index, or -1 if the table is full.
 * data must remain valid for the lifetime of the overlay.
 * Set owns_data via the returned index if the buffer was malloc'd. */
int  mem_overlay_add(memory_t *mem, uint32_t phys_base, uint32_t size,
                     uint8_t *data, rom_type_t type,
                     int cpu_visible, int vic_visible, int active);

/* Activate or deactivate an overlay by index. */
void mem_overlay_set_active(memory_t *mem, int idx, int active);

/* Return the index of the first overlay whose phys_base matches, or -1. */
int  mem_overlay_find(memory_t *mem, uint32_t phys_base);

/* Deactivate and remove all overlays. Frees any owns_data buffers. */
void mem_overlay_clear_all(memory_t *mem);

#endif // MEMORY_H
