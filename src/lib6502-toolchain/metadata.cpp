#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int g_verbose = 0;
#define LOG_V2(...) if (g_verbose >= 2) fprintf(stderr, __VA_ARGS__)
#define LOG_V3(...) if (g_verbose >= 3) fprintf(stderr, __VA_ARGS__)

int load_binary(memory_t *mem, int addr, const char *filename) {
	FILE *bf = fopen(filename, "rb");
	if (!bf) {
		fprintf(stderr, "Warning: cannot open binary '%s': %s\n", filename, strerror(errno));
		return -1;
	}
	fseek(bf, 0, SEEK_END);
	long size = ftell(bf);
	rewind(bf);
	if (mem) {
		for (long i = 0; i < size; i++) {
			int c = fgetc(bf);
			if (c == EOF) { size = i; break; }
			if (addr + (int)i < 65536)
				mem_write(mem, addr + i, (unsigned char)c);
		}
	}
	fclose(bf);
	return (int)size;
}

int load_prg(memory_t *mem, const char *filename, int *out_load_addr) {
    LOG_V2("DEBUG: ENTER load_prg\n");
    LOG_V2("DEBUG: load_prg filename=%s mem=%p\n", filename ? filename : "NULL", (void*)mem);
    if (!filename) return -1;
    FILE *bf = fopen(filename, "rb");
    if (!bf) {
        fprintf(stderr, "Warning: cannot open PRG '%s': %s\n", filename, strerror(errno));
        return -1;
    }
    LOG_V2("DEBUG: PRG opened\n");
    
    unsigned char lo = fgetc(bf);
    unsigned char hi = fgetc(bf);
    int addr = (hi << 8) | lo;
    if (out_load_addr) *out_load_addr = addr;
    LOG_V2("DEBUG: PRG load address: $%04X\n", addr);

    fseek(bf, 0, SEEK_END);
    long size = ftell(bf) - 2;
    fseek(bf, 2, SEEK_SET);
    LOG_V2("DEBUG: PRG size: %ld\n", size);

    if (mem) {
        for (long i = 0; i < size; i++) {
            int c = fgetc(bf);
            if (c == EOF) { size = i; break; }
            if (addr + (int)i < 65536) {
                mem_write(mem, (unsigned short)(addr + i), (unsigned char)c);
            }
        }
    }
    LOG_V2("DEBUG: PRG loaded\n");
    fclose(bf);
    return (int)size;
}

bool load_toolchain_bundle(memory_t *mem, symbol_table_t *st, source_map_t *sm, const char *base_path, int *out_load_addr) {
    LOG_V2("DEBUG: ENTER load_toolchain_bundle mem=%p base=%s\n", (void*)mem, base_path);
    char path[512];

    /* Try .prg first */
    snprintf(path, sizeof(path), "%s.prg", base_path);
    LOG_V2("DEBUG: Attempting to load PRG: %s\n", path);
    int load_addr = 0x0801;
    int n = load_prg(mem, path, &load_addr);
    if (out_load_addr) *out_load_addr = load_addr;
    LOG_V2("DEBUG: load_prg returned %d\n", n);
    if (n < 0) {
        /* Try .bin */
        snprintf(path, sizeof(path), "%s.bin", base_path);
        n = load_binary(mem, 0x0801, path); /* Default to $0801 for bin if not specified */
    }
    
    if (n < 0) return false;

    /* Try .list */
    snprintf(path, sizeof(path), "%s.list", base_path);
    if (sm) source_map_load_acme_list(sm, st, path);
    
    /* Try .sym */
    snprintf(path, sizeof(path), "%s.sym", base_path);
    if (st) symbol_load_file(st, path);

    return true;
}
