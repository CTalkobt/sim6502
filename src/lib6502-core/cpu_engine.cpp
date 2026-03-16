#include "cpu_engine.h"
#include <stdio.h>

unsigned short decode_operand(CPUState *cpu, memory_t *mem, unsigned char mode) {
	switch (mode) {
	case MODE_IMPLIED: return 0;
	case MODE_IMMEDIATE: case MODE_ZP: case MODE_ZP_X: case MODE_ZP_Y:
	case MODE_INDIRECT_X: case MODE_INDIRECT_Y:
	case MODE_ZP_INDIRECT: case MODE_ZP_INDIRECT_Z:
	case MODE_ZP_INDIRECT_FLAT: case MODE_ZP_INDIRECT_Z_FLAT:
	case MODE_SP_INDIRECT_Y: case MODE_RELATIVE: case MODE_ABS_INDIRECT_Y:
		return mem_read(mem, (unsigned short)(cpu->pc + 1));
	case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
	case MODE_INDIRECT: case MODE_ABS_INDIRECT_X:
	case MODE_RELATIVE_LONG: case MODE_IMMEDIATE_WORD:
		return (unsigned short)(mem_read(mem, (unsigned short)(cpu->pc + 1)) | (mem_read(mem, (unsigned short)(cpu->pc + 2)) << 8));
	default: return 0;
	}
}

void execute_from_mem(CPU *cpu, memory_t *mem, const dispatch_table_t *dt, cpu_type_t cpu_type) {
	unsigned char byte0 = mem_read(mem, cpu->pc);
	if (cpu_type == CPU_45GS02) {
		if (byte0 == 0x42) {
			unsigned char byte1 = mem_read(mem, (unsigned short)(cpu->pc + 1));
			if (byte1 == 0x42) {
				unsigned char byte2 = mem_read(mem, (unsigned short)(cpu->pc + 2));
				if (byte2 == 0xEA) {
					unsigned char base = mem_read(mem, (unsigned short)(cpu->pc + 3));
					const dispatch_entry_t *e = &dt->quad_eom[base];
					if (e->fn) { 
						if (cpu->debug) fprintf(stderr, "[DEBUG] Executing QUAD_EOM opcode $%02X at $%04X (%s)\n", base, cpu->pc, e->mnemonic);
						cpu->pc += 3; cpu->eom_prefix = 2; unsigned short arg = decode_operand(cpu, mem, e->mode); e->fn(cpu, mem, arg); return; 
					}
				} else {
					const dispatch_entry_t *e = &dt->quad[byte2];
					if (e->fn) { 
						if (cpu->debug) fprintf(stderr, "[DEBUG] Executing QUAD opcode $%02X at $%04X (%s)\n", byte2, cpu->pc, e->mnemonic);
						cpu->pc += 2; cpu->eom_prefix = (cpu->eom_prefix == 1) ? 2 : 0; unsigned short arg = decode_operand(cpu, mem, e->mode); e->fn(cpu, mem, arg); return; 
					}
				}
			}
		} else if (byte0 == 0xEA) {
			unsigned char byte1 = mem_read(mem, (unsigned short)(cpu->pc + 1));
			const dispatch_entry_t *e = &dt->eom[byte1];
			if (e->fn) {
				if (cpu->debug) fprintf(stderr, "[DEBUG] Executing EOM-prefixed opcode $%02X at $%04X (%s)\n", byte1, cpu->pc, e->mnemonic);
				cpu->pc += 1; cpu->eom_prefix = 2; unsigned short arg = decode_operand(cpu, mem, e->mode); e->fn(cpu, mem, arg); return;
			}
		}
	}
	cpu->eom_prefix = (cpu->eom_prefix == 1) ? 2 : 0;
	const dispatch_entry_t *e = &dt->base[byte0];
	if (!e->fn) {
		if (cpu->debug) fprintf(stderr, "[DEBUG] No handler for opcode $%02X at $%04X (CPU type %d)\n", byte0, cpu->pc, (int)cpu_type);
		cpu->pc++; return;
	}
	if (cpu->debug) fprintf(stderr, "[DEBUG] Executing opcode $%02X at $%04X (%s)\n", byte0, cpu->pc, e->mnemonic);
	unsigned short arg = decode_operand(cpu, mem, e->mode); e->fn(cpu, mem, arg);

	if (mem->io_registry) {
		mem->io_registry->tick_all(cpu->cycles);
	}
}
