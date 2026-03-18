#ifndef MEMORY_TYPES_H
#define MEMORY_TYPES_H

#include <stdint.h>

#define FAR_PAGE_SHIFT  12
#define FAR_PAGE_SIZE   (1 << FAR_PAGE_SHIFT)		/* 4096 bytes per page */
#define FAR_NUM_PAGES   (0x10000000 >> FAR_PAGE_SHIFT)	/* 65536 pages for 28-bit space */

class IOHandler;
class IORegistry;

struct memory_t {
	unsigned char mem[0x10000];
	/* Character ROM: 4 KB (2×256-char sets, 8 bytes/char).
	 * Separate from RAM so it never occupies CPU address space.
	 * CPU sees it at $D000–$DFFF when CHAREN=0, HIRAM=1 (C64 PLA).
	 * VIC-II sees it in bank 0 ($1000–$1FFF) and bank 2 ($9000–$9FFF). */
	unsigned char char_rom[4096];
	int mem_writes;
	unsigned short mem_addr[256];
	unsigned char mem_val[256];     /* value AFTER write  */
	unsigned char mem_old_val[256]; /* value BEFORE write */
	unsigned char *far_pages[FAR_NUM_PAGES];	/* sparse 28-bit page table */
	unsigned int map_offset[8];		/* MAP: per-8KB-block physical offset added to virtual addr; 0 = passthrough */
	IOHandler *io_handlers[0x10000];
	IORegistry *io_registry;
};

typedef struct memory_t memory_t;

#endif // MEMORY_TYPES_H
