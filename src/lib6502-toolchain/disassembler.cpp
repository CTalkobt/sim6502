#include "disassembler.h"
#include "opcodes/opcodes.h"
#include <stdio.h>
#include <string.h>

void dispatch_build(dispatch_table_t *dt,
		const opcode_handler_t *handlers, int n, cpu_type_t cpu_type) {
	fprintf(stderr, "[DEBUG] Building dispatch table for CPU type %d with %d handlers\n", (int)cpu_type, n);
	for (int i = 0; i < n; i++) {
		unsigned char olen = handlers[i].opcode_len;
		if (olen == 0) continue;
		unsigned char cyc;
		switch (cpu_type) {
		case CPU_65C02:  cyc = handlers[i].cycles_65c02  ? handlers[i].cycles_65c02  : handlers[i].cycles_6502; break;
		case CPU_65CE02: cyc = handlers[i].cycles_65ce02 ? handlers[i].cycles_65ce02 : handlers[i].cycles_6502; break;
		case CPU_45GS02: cyc = handlers[i].cycles_45gs02 ? handlers[i].cycles_45gs02 : handlers[i].cycles_6502; break;
		case CPU_6502_UNDOCUMENTED: cyc = handlers[i].cycles_6502; break;
		default:         cyc = handlers[i].cycles_6502; break;
		}
		dispatch_entry_t *slot = NULL;
		unsigned char key = handlers[i].opcode_bytes[olen - 1];
		if (olen == 1) {
			slot = &dt->base[key];
		} else if (olen == 2 && handlers[i].opcode_bytes[0] == 0xEA) {
			continue;
		} else if (olen == 3 && handlers[i].opcode_bytes[0] == 0x42 &&
		           handlers[i].opcode_bytes[1] == 0x42) {
			slot = &dt->quad[key];
		} else if (olen == 4 && handlers[i].opcode_bytes[0] == 0x42 &&
		           handlers[i].opcode_bytes[1] == 0x42 &&
		           handlers[i].opcode_bytes[2] == 0xEA) {
			slot = &dt->quad_eom[key];
		}
		if (slot) {
			slot->fn       = handlers[i].fn;
			slot->mode     = handlers[i].mode;
			slot->mnemonic = handlers[i].mnemonic;
			slot->cycles   = cyc;
		}
	}
}

const dispatch_entry_t *peek_dispatch(const CPUState *cpu, const memory_t *mem,
		const dispatch_table_t *dt, cpu_type_t cpu_type) {
	unsigned char byte0 = mem_read((memory_t *)mem, cpu->pc);
	if (cpu_type == CPU_45GS02 && byte0 == 0x42) {
		unsigned char byte1 = mem_read((memory_t *)mem, (unsigned short)(cpu->pc + 1));
		if (byte1 == 0x42) {
			unsigned char byte2 = mem_read((memory_t *)mem, (unsigned short)(cpu->pc + 2));
			if (byte2 == 0xEA) {
				unsigned char byte3 = mem_read((memory_t *)mem, (unsigned short)(cpu->pc + 3));
				if (dt->quad_eom[byte3].fn) return &dt->quad_eom[byte3];
			} else {
				if (dt->quad[byte2].fn) return &dt->quad[byte2];
			}
		}
	}
	return &dt->base[byte0];
}

int disasm_one(const memory_t *mem, const dispatch_table_t *dt,
                      cpu_type_t cpu_type, unsigned short addr,
                      char *buf, int bufsz) {
    unsigned char b0 = mem->mem[addr];
    const dispatch_entry_t *e = NULL;
    int prefix_len = 0;
    if (cpu_type == CPU_45GS02 && b0 == 0x42) {
        unsigned char b1 = mem->mem[(unsigned short)(addr + 1)];
        if (b1 == 0x42) {
            unsigned char b2 = mem->mem[(unsigned short)(addr + 2)];
            if (b2 == 0xEA) {
                unsigned char b3 = mem->mem[(unsigned short)(addr + 3)];
                if (dt->quad_eom[b3].fn) { e = &dt->quad_eom[b3]; prefix_len = 3; }
            } else {
                if (dt->quad[b2].fn) { e = &dt->quad[b2]; prefix_len = 2; }
            }
        }
    }
    if (!e) {
        const dispatch_entry_t *be = &dt->base[b0];
        if (be->fn) e = be;
    }
    if (!e) {
        snprintf(buf, bufsz, "$%04X: %02X                    .byte $%02X", addr, b0, b0);
        return 1;
    }
    int instr_len  = get_instruction_length(e->mode);
    int total_len  = prefix_len + instr_len;
    unsigned char op1 = (instr_len >= 2) ? mem->mem[(unsigned short)(addr + prefix_len + 1)] : 0;
    unsigned char op2 = (instr_len >= 3) ? mem->mem[(unsigned short)(addr + prefix_len + 2)] : 0;
    unsigned short operand = (unsigned short)(op1 | (op2 << 8));
    char hexbuf[24] = "";
    int hpos = 0;
    for (int i = 0; i < total_len; i++)
        hpos += snprintf(hexbuf + hpos, (int)sizeof(hexbuf) - hpos,
                         "%02X ", mem->mem[(unsigned short)(addr + i)]);
    char opstr[32] = "";
    switch (e->mode) {
    case MODE_IMPLIED:                                                          break;
    case MODE_IMMEDIATE:        snprintf(opstr, sizeof(opstr), "#$%02X",   op1);       break;
    case MODE_IMMEDIATE_WORD:   snprintf(opstr, sizeof(opstr), "#$%04X",   operand);   break;
    case MODE_ZP:               snprintf(opstr, sizeof(opstr), "$%02X",    op1);       break;
    case MODE_ZP_X:             snprintf(opstr, sizeof(opstr), "$%02X,X",  op1);       break;
    case MODE_ZP_Y:             snprintf(opstr, sizeof(opstr), "$%02X,Y",  op1);       break;
    case MODE_ABSOLUTE:         snprintf(opstr, sizeof(opstr), "$%04X",    operand);   break;
    case MODE_ABSOLUTE_X:       snprintf(opstr, sizeof(opstr), "$%04X,X",  operand);   break;
    case MODE_ABSOLUTE_Y:       snprintf(opstr, sizeof(opstr), "$%04X,Y",  operand);   break;
    case MODE_INDIRECT:         snprintf(opstr, sizeof(opstr), "($%04X)",  operand);   break;
    case MODE_INDIRECT_X:       snprintf(opstr, sizeof(opstr), "($%02X,X)",op1);       break;
    case MODE_INDIRECT_Y:       snprintf(opstr, sizeof(opstr), "($%02X),Y",op1);       break;
    case MODE_ZP_INDIRECT:      snprintf(opstr, sizeof(opstr), "($%02X)",  op1);       break;
    case MODE_ABS_INDIRECT_Y:   snprintf(opstr, sizeof(opstr), "($%04X),Y",operand);   break;
    case MODE_ZP_INDIRECT_Z:    snprintf(opstr, sizeof(opstr), "($%02X),Z",op1);       break;
    case MODE_SP_INDIRECT_Y:    snprintf(opstr, sizeof(opstr), "($%02X,SP),Y", op1);   break;
    case MODE_ABS_INDIRECT_X:   snprintf(opstr, sizeof(opstr), "($%04X,X)",operand);   break;
    case MODE_ZP_INDIRECT_FLAT: snprintf(opstr, sizeof(opstr), "[$%02X]",  op1);       break;
    case MODE_ZP_INDIRECT_Z_FLAT: snprintf(opstr, sizeof(opstr), "[$%02X],Z", op1);    break;
    case MODE_RELATIVE: {
        int target = (int)(addr + prefix_len) + instr_len + (signed char)op1;
        snprintf(opstr, sizeof(opstr), "$%04X", (unsigned short)target);
        break;
    }
    case MODE_RELATIVE_LONG: {
        int target = (int)(addr + prefix_len) + instr_len + (short)operand;
        snprintf(opstr, sizeof(opstr), "$%04X", (unsigned short)target);
        break;
    }
    default:
        snprintf(opstr, sizeof(opstr), "?");
        break;
    }
    snprintf(buf, bufsz, "$%04X: %-18s %-6s %s", addr, hexbuf, e->mnemonic, opstr);
    return total_len;
}

int disasm_one_entry(const memory_t *mem, const dispatch_table_t *dt,
                     cpu_type_t cpu_type, unsigned short addr,
                     disasm_entry_t *out) {
    unsigned char b0 = mem->mem[addr];
    const dispatch_entry_t *e = NULL;
    int prefix_len = 0;
    if (cpu_type == CPU_45GS02 && b0 == 0x42) {
        unsigned char b1 = mem->mem[(unsigned short)(addr + 1)];
        if (b1 == 0x42) {
            unsigned char b2 = mem->mem[(unsigned short)(addr + 2)];
            if (b2 == 0xEA) {
                unsigned char b3 = mem->mem[(unsigned short)(addr + 3)];
                if (dt->quad_eom[b3].fn) { e = &dt->quad_eom[b3]; prefix_len = 3; }
            } else {
                if (dt->quad[b2].fn) { e = &dt->quad[b2]; prefix_len = 2; }
            }
        }
    }
    if (!e) {
        const dispatch_entry_t *be = &dt->base[b0];
        if (be->fn) e = be;
    }

    out->address = addr;
    out->cycles  = e ? e->cycles : 0;

    if (!e) {
        snprintf(out->bytes,    sizeof(out->bytes),    "%02X", b0);
        snprintf(out->mnemonic, sizeof(out->mnemonic), ".byte");
        snprintf(out->operand,  sizeof(out->operand),  "$%02X", b0);
        out->size = 1;
        return 1;
    }

    int instr_len = get_instruction_length(e->mode);
    int total_len = prefix_len + instr_len;
    out->size = total_len;

    /* Build hex byte string */
    int hpos = 0;
    for (int i = 0; i < total_len && hpos < (int)sizeof(out->bytes) - 3; i++) {
        if (i > 0) out->bytes[hpos++] = ' ';
        hpos += snprintf(out->bytes + hpos, sizeof(out->bytes) - (size_t)hpos,
                         "%02X", mem->mem[(unsigned short)(addr + i)]);
    }

    snprintf(out->mnemonic, sizeof(out->mnemonic), "%s", e->mnemonic);

    unsigned char op1 = (instr_len >= 2) ? mem->mem[(unsigned short)(addr + prefix_len + 1)] : 0;
    unsigned char op2 = (instr_len >= 3) ? mem->mem[(unsigned short)(addr + prefix_len + 2)] : 0;
    unsigned short operand = (unsigned short)(op1 | (op2 << 8));

    switch (e->mode) {
    case MODE_IMPLIED:
        out->operand[0] = '\0';
        break;
    case MODE_IMMEDIATE:
        snprintf(out->operand, sizeof(out->operand), "#$%02X", op1);
        break;
    case MODE_IMMEDIATE_WORD:
        snprintf(out->operand, sizeof(out->operand), "#$%04X", operand);
        break;
    case MODE_ZP:
        snprintf(out->operand, sizeof(out->operand), "$%02X", op1);
        break;
    case MODE_ZP_X:
        snprintf(out->operand, sizeof(out->operand), "$%02X,X", op1);
        break;
    case MODE_ZP_Y:
        snprintf(out->operand, sizeof(out->operand), "$%02X,Y", op1);
        break;
    case MODE_ABSOLUTE:
        snprintf(out->operand, sizeof(out->operand), "$%04X", operand);
        break;
    case MODE_ABSOLUTE_X:
        snprintf(out->operand, sizeof(out->operand), "$%04X,X", operand);
        break;
    case MODE_ABSOLUTE_Y:
        snprintf(out->operand, sizeof(out->operand), "$%04X,Y", operand);
        break;
    case MODE_INDIRECT:
        snprintf(out->operand, sizeof(out->operand), "($%04X)", operand);
        break;
    case MODE_INDIRECT_X:
        snprintf(out->operand, sizeof(out->operand), "($%02X,X)", op1);
        break;
    case MODE_INDIRECT_Y:
        snprintf(out->operand, sizeof(out->operand), "($%02X),Y", op1);
        break;
    case MODE_ZP_INDIRECT:
        snprintf(out->operand, sizeof(out->operand), "($%02X)", op1);
        break;
    case MODE_ABS_INDIRECT_Y:
        snprintf(out->operand, sizeof(out->operand), "($%04X),Y", operand);
        break;
    case MODE_ZP_INDIRECT_Z:
        snprintf(out->operand, sizeof(out->operand), "($%02X),Z", op1);
        break;
    case MODE_SP_INDIRECT_Y:
        snprintf(out->operand, sizeof(out->operand), "($%02X,SP),Y", op1);
        break;
    case MODE_ABS_INDIRECT_X:
        snprintf(out->operand, sizeof(out->operand), "($%04X,X)", operand);
        break;
    case MODE_ZP_INDIRECT_FLAT:
        snprintf(out->operand, sizeof(out->operand), "[$%02X]", op1);
        break;
    case MODE_ZP_INDIRECT_Z_FLAT:
        snprintf(out->operand, sizeof(out->operand), "[$%02X],Z", op1);
        break;
    case MODE_RELATIVE: {
        int target = (int)(addr + prefix_len) + instr_len + (signed char)op1;
        snprintf(out->operand, sizeof(out->operand), "$%04X", (unsigned short)target);
        break;
    }
    case MODE_RELATIVE_LONG: {
        int target = (int)(addr + prefix_len) + instr_len + (short)operand;
        snprintf(out->operand, sizeof(out->operand), "$%04X", (unsigned short)target);
        break;
    }
    default:
        snprintf(out->operand, sizeof(out->operand), "?");
        break;
    }
    return total_len;
}
