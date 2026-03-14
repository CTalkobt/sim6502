#ifndef METADATA_H
#define METADATA_H

#include "memory.h"
#include "symbols.h"
#include "list_parser.h"
#include "cpu_types.h"
#include "machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load a binary file (.bin) to a specific address */
int load_binary(memory_t *mem, int addr, const char *filename);

/* Load a Commodore-style PRG file (first two bytes are load address) */
int load_prg(memory_t *mem, const char *filename, int *out_load_addr);

/* Load a toolchain bundle: .bin/.prg + .list + .sym
   If out_load_addr is non-NULL, receives the PRG load address.
   If err_msg is non-NULL, receives any error message on failure. */
bool load_toolchain_bundle(memory_t *mem, symbol_table_t *st, source_map_t *sm, const char *base_path, int *out_load_addr, char *err_msg, int err_sz);

/* Scan an .asm source file for the first .cpu or //.cpu directive and return
   the corresponding cpu_type_t.  Returns CPU_6502 if no directive is found.
   asm_path must be the full path including the .asm extension. */
cpu_type_t detect_asm_cpu_type(const char *asm_path);

/* Read cpu/machine type from SYM_PROCESSOR symbols previously loaded into a
   symbol table (e.g. by load_toolchain_bundle or load_companion_files).
   Returns CPU_6502 / MACHINE_C64 if no relevant entry is present. */
cpu_type_t     cpu_type_from_symbols(const symbol_table_t *st);
machine_type_t machine_type_from_symbols(const symbol_table_t *st);

/* Load companion annotation files (.list, .sym, .sym_add) for a base path.
   Useful when a .prg/.bin was loaded directly and companion files should
   still be applied.  base_path must have the extension already stripped.
   .sym_add accepts both KickAssembler symbol format and SIM_INSPECT/SIM_TRAP lines. */
void load_companion_files(symbol_table_t *st, source_map_t *sm, const char *base_path);

#ifdef __cplusplus
}
#endif

#endif
