#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
    FILE *bf = fopen(filename, "rb");
    if (!bf) {
        fprintf(stderr, "Warning: cannot open PRG '%s': %s\n", filename, strerror(errno));
        return -1;
    }
    
    unsigned char lo = fgetc(bf);
    unsigned char hi = fgetc(bf);
    int addr = (hi << 8) | lo;
    if (out_load_addr) *out_load_addr = addr;

    fseek(bf, 0, SEEK_END);
    long size = ftell(bf) - 2;
    fseek(bf, 2, SEEK_SET);

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

bool load_toolchain_bundle(memory_t *mem, symbol_table_t *st, source_map_t *sm, const char *base_path) {
    char path[512];
    
    /* Try .prg first */
    snprintf(path, sizeof(path), "%s.prg", base_path);
    int load_addr = 0x0801;
    int n = load_prg(mem, path, &load_addr);
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
