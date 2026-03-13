#ifndef METADATA_H
#define METADATA_H

#include "memory.h"
#include "symbols.h"
#include "list_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load a binary file (.bin) to a specific address */
int load_binary(memory_t *mem, int addr, const char *filename);

/* Load a Commodore-style PRG file (first two bytes are load address) */
int load_prg(memory_t *mem, const char *filename, int *out_load_addr);

/* Load a toolchain bundle: .bin/.prg + .list + .sym
   If out_load_addr is non-NULL, receives the PRG load address. */
bool load_toolchain_bundle(memory_t *mem, symbol_table_t *st, source_map_t *sm, const char *base_path, int *out_load_addr);

#ifdef __cplusplus
}
#endif

#endif
