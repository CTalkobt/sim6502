#ifndef DISPATCH_H
#define DISPATCH_H

#include "cpu_types.h"  /* for cpu_type_t */

/* Forward declarations — dispatch types depend on CPU and memory_t only by pointer. */
class CPU;
struct memory_t;

/* Opcode handler function pointer. */
typedef void (*opcode_fn)(CPU *cpu, memory_t *mem, unsigned short arg);

/* Single dispatch slot: one opcode byte maps to this entry. */
typedef struct {
	opcode_fn     fn;
	unsigned char mode;
	const char   *mnemonic;
	unsigned char cycles;
} dispatch_entry_t;

/* Full dispatch table: base, quad-prefix, and quad-EOM-prefix slots. */
typedef struct {
	dispatch_entry_t base[256];     /* indexed by single opcode byte */
	dispatch_entry_t eom[256];      /* 45GS02: $EA prefix, indexed by 2nd byte */
	dispatch_entry_t quad[256];     /* 45GS02: $42 $42 prefix, indexed by 3rd byte */
	dispatch_entry_t quad_eom[256]; /* 45GS02: $42 $42 $EA prefix, indexed by 4th byte */
} dispatch_table_t;

/* Forward declaration of opcode_handler_t (defined in opcodes.h). */
struct opcode_handler_t;

/* Build a dispatch table from an array of opcode handlers. */
void dispatch_build(dispatch_table_t *dt, const opcode_handler_t *handlers, int n, cpu_type_t cpu_type);

#endif
