#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "memory.h"

/* --- Memory Viewer / Inspector Structures --- */

typedef struct {
	uint16_t start_addr;
	uint16_t end_addr;
	int enabled;
} watch_range_t;

#define MAX_WATCH_RANGES 8

typedef struct {
	watch_range_t ranges[MAX_WATCH_RANGES];
	int num_ranges;
	int show_all;
} memory_viewer_t;

typedef struct {
	uint16_t address;
	uint8_t last_value;
	int watching;
	char name[64];
} watch_t;

typedef struct {
	watch_t watches[16];
	int count;
} watch_list_t;

/* --- Initialization --- */

static inline void memory_viewer_init(memory_viewer_t *viewer) {
	viewer->num_ranges = 0;
	viewer->show_all = 0;
	for (int i = 0; i < MAX_WATCH_RANGES; i++) {
		viewer->ranges[i].enabled = 0;
	}
}

static inline void watch_list_init(watch_list_t *wl) {
	wl->count = 0;
	memset(wl->watches, 0, sizeof(wl->watches));
}

/* --- Range Management --- */

static inline int memory_viewer_add_range(memory_viewer_t *viewer, uint16_t start, uint16_t end) {
	if (viewer->num_ranges >= MAX_WATCH_RANGES) return 0;
	if (start > end) { uint16_t tmp = start; start = end; end = tmp; }
	viewer->ranges[viewer->num_ranges].start_addr = start;
	viewer->ranges[viewer->num_ranges].end_addr = end;
	viewer->ranges[viewer->num_ranges].enabled = 1;
	viewer->num_ranges++;
	return 1;
}

/* --- Inspection / Display --- */

static inline void memory_dump(const memory_t *mem, uint16_t start, uint16_t end) {
	printf("\nMemory Dump: 0x%04X to 0x%04X\n", start, end);
	printf("═══════════════════════════════════════════════════════════════\n");
	printf("Address  | Hex Values                          | ASCII\n");
	printf("─────────┼─────────────────────────────────────┼─────────────────\n");
	
	for (uint32_t addr = (start & 0xFFF0); addr <= end; addr += 16) {
		printf("%04X     | ", (uint16_t)addr);
		
		/* Hex values */
		for (int i = 0; i < 16; i++) {
			if (addr + i >= start && addr + i <= end) {
				printf("%02X ", mem->mem[(uint16_t)(addr + i)]);
			} else {
				printf("   ");
			}
			if (i == 7) printf("  ");
		}
		
		printf("| ");
		
		/* ASCII representation */
		for (int i = 0; i < 16; i++) {
			if (addr + i >= start && addr + i <= end) {
				uint8_t c = mem->mem[(uint16_t)(addr + i)];
				printf("%c", (c >= 32 && c < 127) ? c : '.');
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
	printf("═══════════════════════════════════════════════════════════════\n");
}

static inline void memory_peek(const memory_t *mem, uint16_t addr) {
	uint8_t val = mem->mem[addr];
	printf("Peek at 0x%04X: 0x%02X (%d, '%c')\n", 
		addr, val, val, (val >= 32 && val < 127) ? val : '.');
}

static inline void memory_read_word(const memory_t *mem, uint16_t addr) {
	uint8_t lo = mem->mem[addr];
	uint8_t hi = mem->mem[addr + 1];
	uint16_t word = lo | (hi << 8);
	printf("Memory[0x%04X:0x%04X] = 0x%04X (%d)\n", addr, (uint16_t)(addr + 1), word, word);
}

/* --- Watch Points --- */

static inline int watch_add(watch_list_t *wl, uint16_t addr, const char *name) {
	if (wl->count >= 16) return 0;
	wl->watches[wl->count].address = addr;
	wl->watches[wl->count].watching = 1;
	wl->watches[wl->count].last_value = 0;
	strncpy(wl->watches[wl->count].name, name, sizeof(wl->watches[0].name) - 1);
	wl->count++;
	return 1;
}

static inline void watch_update(watch_list_t *wl, const memory_t *mem) {
	for (int i = 0; i < wl->count; i++) {
		if (!wl->watches[i].watching) continue;
		uint8_t current = mem->mem[wl->watches[i].address];
		if (current != wl->watches[i].last_value) {
			printf("[WATCH] %s (0x%04X) changed: 0x%02X -> 0x%02X\n", 
				wl->watches[i].name, wl->watches[i].address, wl->watches[i].last_value, current);
			wl->watches[i].last_value = current;
		}
	}
}

/* --- Utilities --- */

static inline int memory_search(const memory_t *mem, uint16_t start, uint16_t end,
	uint8_t pattern, uint16_t *result) {
	for (uint32_t addr = start; addr <= end; addr++) {
		if (mem->mem[(uint16_t)addr] == pattern) {
			*result = (uint16_t)addr;
			return 1;
		}
	}
	return 0;
}

static inline int memview_parse_range(const char *range_str, uint16_t *start, uint16_t *end) {
	char temp[256];
	strncpy(temp, range_str, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = 0;
	char *delim = strchr(temp, ':');
	if (!delim) delim = strchr(temp, '-');
	if (!delim) return 0;
	*delim = 0;
	*start = (uint16_t)strtol(temp, NULL, 16);
	*end = (uint16_t)strtol(delim + 1, NULL, 16);
	return (*start <= *end);
}

#endif
