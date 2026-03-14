#ifndef MEMORY_TYPES_H
#define MEMORY_TYPES_H

#include <stdint.h>

#define FAR_PAGE_SHIFT  12
#define FAR_PAGE_SIZE   (1 << FAR_PAGE_SHIFT)		/* 4096 bytes per page */
#define FAR_NUM_PAGES   (0x10000000 >> FAR_PAGE_SHIFT)	/* 65536 pages for 28-bit space */

class IOHandler;
class IORegistry;

/* ROM type tag for overlay entries */
typedef enum {
    ROM_TYPE_NONE      = 0,
    ROM_TYPE_KERNAL    = 1,
    ROM_TYPE_BASIC     = 2,
    ROM_TYPE_CHARACTER = 3,
    ROM_TYPE_EXPANSION = 4,
    ROM_TYPE_OTHER     = 5
} rom_type_t;

#define MAX_OVERLAYS 16

/* A ROM overlay: a fixed region of read-only data mapped over RAM.
 * cpu_visible=1: scanned by mem_read() before I/O handlers or RAM.
 * vic_visible=1: scanned by vic_read() for VIC character/bitmap reads.
 * active is toggled at runtime by device handlers (e.g. CharRomPlaHandler).
 * owns_data=1: data buffer was malloc'd and must be freed on clear. */
typedef struct {
    uint32_t   phys_base;    /* start address (CPU/VIC logical space) */
    uint32_t   size;         /* size in bytes */
    uint8_t   *data;         /* pointer to ROM data */
    rom_type_t type;
    int        active;       /* 1 = currently mapped */
    int        cpu_visible;  /* 1 = checked on CPU mem_read() */
    int        vic_visible;  /* 1 = checked on VIC vic_read() */
    int        owns_data;    /* 1 = data was malloc'd; free on clear */
} mem_overlay_t;

struct memory_t {
	unsigned char mem[0x10000];
	/* Character ROM: 4 KB backing store for the built-in charset overlays.
	 * Never occupies CPU address space directly — accessed via mem_overlay_t.
	 * C64PlaHandler ($00/$01) activates the $D000 CHARACTER overlay when
	 * CHAREN=0 and (HIRAM=1 or LORAM=1).  VIC bank-0/bank-2 windows at
	 * $1000 and $9000 are always-active vic_visible overlays. */
	unsigned char char_rom[4096];
	int mem_writes;
	unsigned short mem_addr[256];
	unsigned char mem_val[256];     /* value AFTER write  */
	unsigned char mem_old_val[256]; /* value BEFORE write */
	unsigned char *far_pages[FAR_NUM_PAGES];	/* sparse 28-bit page table */
	unsigned int map_offset[8];		/* MAP: per-8KB-block physical offset added to virtual addr; 0 = passthrough */
	IOHandler *io_handlers[0x10000];
	IORegistry *io_registry;
	mem_overlay_t overlays[MAX_OVERLAYS];
	int           overlay_count;
};

typedef struct memory_t memory_t;

#endif // MEMORY_TYPES_H
