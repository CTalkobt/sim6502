#ifndef OPCODES_H
#define OPCODES_H

#include "cpu.h"
#include "memory.h"
#include "dispatch.h"

typedef struct opcode_handler_t {
	const char *mnemonic;
	unsigned char mode;
	opcode_fn fn;
	unsigned char cycles_6502;      /* Base cycles on 6502 */
	unsigned char cycles_65c02;     /* Cycles on 65C02 (may differ) */
	unsigned char cycles_65ce02;    /* Cycles on 65CE02 (may differ) */
	unsigned char cycles_45gs02;    /* Cycles on 45GS02 (may differ) */
	unsigned char opcode_bytes[4];  /* Opcode bytes first→last: 1=plain, 2={EOM,base},
	                                   3={$42,$42,base}, 4={$42,$42,$EA,base} */
	unsigned char opcode_len;       /* 0=unassigned, 1–4=number of opcode bytes */
} opcode_handler_t;

extern void op_brk(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void op_rti(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void op_nop(cpu_t *cpu, memory_t *mem, unsigned short arg);

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

static inline int get_instruction_length(unsigned char mode) {
	switch (mode) {
	case MODE_IMPLIED:            return 1;
	case MODE_IMMEDIATE:          return 2;
	case MODE_ZP:                 return 2;
	case MODE_ZP_X:               return 2;
	case MODE_ZP_Y:               return 2;
	case MODE_INDIRECT_X:         return 2;
	case MODE_INDIRECT_Y:         return 2;
	case MODE_ZP_INDIRECT:        return 2;
	case MODE_ZP_INDIRECT_Z:      return 2;
	case MODE_ZP_INDIRECT_FLAT:   return 2;
	case MODE_ZP_INDIRECT_Z_FLAT: return 2;
	case MODE_SP_INDIRECT_Y:      return 2;
	case MODE_RELATIVE:           return 2;
	case MODE_ABSOLUTE:           return 3;
	case MODE_ABSOLUTE_X:         return 3;
	case MODE_ABSOLUTE_Y:         return 3;
	case MODE_INDIRECT:           return 3;
	case MODE_ABS_INDIRECT_X:     return 3;
	case MODE_ABS_INDIRECT_Y:     return 3;
	case MODE_RELATIVE_LONG:      return 3;
	case MODE_IMMEDIATE_WORD:     return 3;
	default:                      return 1;
	}
}

static inline const char *mode_name(unsigned char mode) {
	switch (mode) {
	case MODE_IMPLIED:            return "implied";
	case MODE_IMMEDIATE:          return "immediate";
	case MODE_IMMEDIATE_WORD:     return "immediate_word";
	case MODE_ZP:                 return "zp";
	case MODE_ZP_X:               return "zp_x";
	case MODE_ZP_Y:               return "zp_y";
	case MODE_INDIRECT_X:         return "indirect_x";
	case MODE_INDIRECT_Y:         return "indirect_y";
	case MODE_ZP_INDIRECT:        return "zp_indirect";
	case MODE_ZP_INDIRECT_Z:      return "zp_indirect_z";
	case MODE_ZP_INDIRECT_FLAT:   return "zp_indirect_flat";
	case MODE_ZP_INDIRECT_Z_FLAT: return "zp_indirect_z_flat";
	case MODE_SP_INDIRECT_Y:      return "sp_indirect_y";
	case MODE_RELATIVE:           return "relative";
	case MODE_RELATIVE_LONG:      return "relative_long";
	case MODE_ABSOLUTE:           return "absolute";
	case MODE_ABSOLUTE_X:         return "absolute_x";
	case MODE_ABSOLUTE_Y:         return "absolute_y";
	case MODE_INDIRECT:           return "indirect";
	case MODE_ABS_INDIRECT_X:     return "abs_indirect_x";
	case MODE_ABS_INDIRECT_Y:     return "abs_indirect_y";
	default:                      return "unknown";
	}
}

#endif
