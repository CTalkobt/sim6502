#ifndef OPCODES_H
#define OPCODES_H

#include "cpu.h"
#include "memory.h"

typedef void (*opcode_fn)(cpu_t *cpu, memory_t *mem, unsigned short arg);

typedef struct {
	const char *mnemonic;
	unsigned char mode;
	opcode_fn fn;
	unsigned char cycles_6502;      /* Base cycles on 6502 */
	unsigned char cycles_65c02;     /* Cycles on 65C02 (may differ) */
	unsigned char cycles_65ce02;    /* Cycles on 65CE02 (may differ) */
	unsigned char cycles_45gs02;    /* Cycles on 45GS02 (may differ) */
	unsigned int opcode;            /* Opcode byte value (supports multi-byte) */
} opcode_handler_t;

extern opcode_handler_t opcodes_6502[];
extern int OPCODES_6502_COUNT;

extern opcode_handler_t opcodes_6502_undoc[];
extern int OPCODES_6502_UNDOC_COUNT;

extern opcode_handler_t opcodes_65c02[];
extern int OPCODES_65C02_COUNT;

extern opcode_handler_t opcodes_65ce02[];
extern int OPCODES_65CE02_COUNT;

extern opcode_handler_t opcodes_45gs02[];
extern int OPCODES_45GS02_COUNT;

#endif
