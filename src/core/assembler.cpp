#include "assembler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

const char *mode_name(unsigned char mode) {
	switch (mode) {
	case MODE_IMPLIED: return "IMP";
	case MODE_IMMEDIATE: return "IMM";
	case MODE_ABSOLUTE: return "ABS";
	case MODE_ABSOLUTE_X: return "ABX";
	case MODE_ABSOLUTE_Y: return "ABY";
	case MODE_INDIRECT: return "IND";
	case MODE_INDIRECT_X: return "INX";
	case MODE_INDIRECT_Y: return "INY";
	case MODE_ZP: return "ZP";
	case MODE_ZP_X: return "ZPX";
	case MODE_ZP_Y: return "ZPY";
	case MODE_RELATIVE: return "REL";
	case MODE_RELATIVE_LONG: return "RELL";
	case MODE_ZP_INDIRECT: return "ZPI";
	case MODE_ABS_INDIRECT_Y: return "AIY";
	case MODE_ZP_INDIRECT_Z: return "ZIZ";
	case MODE_SP_INDIRECT_Y: return "SIY";
	case MODE_ABS_INDIRECT_X: return "AIX";
	case MODE_IMMEDIATE_WORD: return "IMW";
	case MODE_ZP_INDIRECT_FLAT: return "FLT";
	case MODE_ZP_INDIRECT_Z_FLAT: return "FLZ";
	default: return "???";
	}
}

int get_encoded_length(const char *mnem, unsigned char mode,
		const opcode_handler_t *handlers, int n, cpu_type_t cpu_type) {
	if (strcmp(mnem, "BRK") == 0) return (cpu_type == CPU_45GS02) ? 1 : 2;
	for (int i = 0; i < n; i++) {
		if (strcmp(handlers[i].mnemonic, mnem) == 0 &&
		    handlers[i].mode == mode &&
		    handlers[i].opcode_len > 0) {
			int operand = 0;
			switch (mode) {
			case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
			case MODE_INDIRECT: case MODE_ABS_INDIRECT_X: case MODE_ABS_INDIRECT_Y:
			case MODE_RELATIVE_LONG: case MODE_IMMEDIATE_WORD:
				operand = 2; break;
			case MODE_IMPLIED:
				operand = 0; break;
			default:
				operand = 1; break;
			}
			return (int)handlers[i].opcode_len + operand;
		}
	}
	return get_instruction_length(mode);
}

int get_instruction_length(unsigned char mode) {
	switch (mode) {
	case MODE_IMPLIED: return 1;
	case MODE_IMMEDIATE: return 2;
	case MODE_ZP: return 2;
	case MODE_ZP_X: return 2;
	case MODE_ZP_Y: return 2;
	case MODE_RELATIVE: return 2;
	case MODE_RELATIVE_LONG: return 3;
	case MODE_INDIRECT_X: return 2;
	case MODE_INDIRECT_Y: return 2;
	case MODE_ABSOLUTE: return 3;
	case MODE_ABSOLUTE_X: return 3;
	case MODE_ABSOLUTE_Y: return 3;
	case MODE_INDIRECT: return 3;
	case MODE_ZP_INDIRECT: return 2;
	case MODE_ABS_INDIRECT_Y: return 3;
	case MODE_ZP_INDIRECT_Z: return 2;
	case MODE_SP_INDIRECT_Y: return 2;
	case MODE_ABS_INDIRECT_X: return 3;
	case MODE_IMMEDIATE_WORD: return 3;
	case MODE_ZP_INDIRECT_FLAT: return 2;
	case MODE_ZP_INDIRECT_Z_FLAT: return 2;
	default: return 1;
	}
}

int is_branch_opcode(const char *op) {
	if (strcmp(op, "BCC") == 0 || strcmp(op, "LBCC") == 0) return 1;
	if (strcmp(op, "BCS") == 0 || strcmp(op, "LBCS") == 0) return 1;
	if (strcmp(op, "BEQ") == 0 || strcmp(op, "LBEQ") == 0) return 1;
	if (strcmp(op, "BMI") == 0 || strcmp(op, "LBMI") == 0) return 1;
	if (strcmp(op, "BNE") == 0 || strcmp(op, "LBNE") == 0) return 1;
	if (strcmp(op, "BPL") == 0 || strcmp(op, "LBPL") == 0) return 1;
	if (strcmp(op, "BVC") == 0 || strcmp(op, "LBVC") == 0) return 1;
	if (strcmp(op, "BVS") == 0 || strcmp(op, "LBVS") == 0) return 1;
	if (strcmp(op, "BRA") == 0 || strcmp(op, "LBRA") == 0) return 1;
	if (strcmp(op, "BSR") == 0 || strcmp(op, "LBSR") == 0) return 1;
	if (strcmp(op, "BRL") == 0 || strcmp(op, "LBRL") == 0) return 1;
	return 0;
}

int is_long_branch_opcode(const char *op) {
	if (op[0] == 'L' && (strcmp(op, "LBRL") != 0)) {
		if (strcmp(op, "LBCC") == 0 || strcmp(op, "LBCS") == 0 || 
		    strcmp(op, "LBEQ") == 0 || strcmp(op, "LBNE") == 0 ||
		    strcmp(op, "LBMI") == 0 || strcmp(op, "LBPL") == 0 ||
		    strcmp(op, "LBVC") == 0 || strcmp(op, "LBVS") == 0 ||
		    strcmp(op, "LBRA") == 0 || strcmp(op, "LBSR") == 0)
			return 1;
	}
	if (strcmp(op, "BSR") == 0) return 1;
	if (strcmp(op, "BRL") == 0) return 1;
	return 0;
}

unsigned short parse_value(const char *str, int *out_digits) {
	unsigned short val;
	int digits;
	if (str[0] == '$') {
		const char *start = str + 1;
		val = (unsigned short)strtol(start, NULL, 16);
		digits = 0;
		while (isxdigit(start[digits])) digits++;
	} else if (str[0] == '%') {
		val = (unsigned short)strtol(str + 1, NULL, 2);
		digits = (val <= 0xFF) ? 2 : 4;
	} else if (str[0] == '\'') {
		val = (unsigned short)(unsigned char)str[1];
		digits = 2;
	} else {
		val = (unsigned short)strtol(str, NULL, 10);
		digits = (val <= 0xFF) ? 2 : 4;
	}
	if (out_digits) *out_digits = digits;
	return val;
}

int load_binary_to_mem(memory_t *mem, int addr, const char *filename) {
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
				mem->mem[addr + (int)i] = (unsigned char)c;
		}
	}
	fclose(bf);
	return (int)size;
}

void handle_pseudo_op(const char *line, cpu_type_t *cpu_type, int *pc,
                              memory_t *mem, symbol_table_t *symbols,
                              struct source_stack *ss) {
    (void)ss;
	while (*line && isspace(*line)) line++;
	if (*line != '.') return;
	line++;
	if (strncmp(line, "processor", 9) == 0 && (!line[9] || isspace(line[9]))) {
		line += 9;
		while (*line && isspace(*line)) line++;
		if (strncmp(line, "45gs02", 6) == 0 || strncmp(line, "45GS02", 6) == 0)
			*cpu_type = CPU_45GS02;
		else if (strncmp(line, "65ce02", 6) == 0 || strncmp(line, "65CE02", 6) == 0)
			*cpu_type = CPU_65CE02;
		else if (strncmp(line, "65c02", 5) == 0 || strncmp(line, "65C02", 5) == 0)
			*cpu_type = CPU_65C02;
		else if (strncmp(line, "6502", 4) == 0) {
			line += 4;
			while (*line && isspace(*line)) line++;
			if (strncmp(line, "undoc", 5) == 0)
				*cpu_type = CPU_6502_UNDOCUMENTED;
			else
				*cpu_type = CPU_6502;
		}
		return;
	}
	if (strncmp(line, "org", 3) == 0 && (!line[3] || isspace(line[3]))) {
		line += 3;
		while (*line && isspace(*line)) line++;
		*pc = (int)parse_value(line, NULL);
		return;
	}
	if (strncmp(line, "byte", 4) == 0 && (!line[4] || isspace(line[4]))) {
		line += 4;
		while (*line && isspace(*line)) line++;
		while (*line && *line != ';' && *line != '\n' && *line != '\r') {
			while (*line && isspace(*line)) line++;
			if (!*line || *line == ';' || *line == '\n' || *line == '\r') break;
			unsigned char val;
			if (isalpha((unsigned char)*line) || *line == '_') {
				char lbl[64]; int j = 0;
				while ((isalnum((unsigned char)*line) || *line == '_') && j < 63)
					lbl[j++] = *line++;
				lbl[j] = 0;
				unsigned short addr = 0;
				if (symbols) symbol_lookup_name(symbols, lbl, &addr);
				val = (unsigned char)(addr & 0xFF);
			} else {
				val = (unsigned char)parse_value(line, NULL);
				while (*line && !isspace((unsigned char)*line) &&
				       *line != ',' && *line != ';') line++;
			}
			if (mem) mem->mem[(*pc)++] = val;
			else (*pc)++;
			while (*line && isspace((unsigned char)*line)) line++;
			if (*line == ',') line++;
		}
		return;
	}
	if (strncmp(line, "word", 4) == 0 && (!line[4] || isspace(line[4]))) {
		line += 4;
		while (*line && isspace(*line)) line++;
		while (*line && *line != ';' && *line != '\n' && *line != '\r') {
			while (*line && isspace(*line)) line++;
			if (!*line || *line == ';' || *line == '\n' || *line == '\r') break;
			unsigned short val;
			if (isalpha((unsigned char)*line) || *line == '_') {
				char lbl[64]; int j = 0;
				while ((isalnum((unsigned char)*line) || *line == '_') && j < 63)
					lbl[j++] = *line++;
				lbl[j] = 0;
				val = 0;
				if (symbols) symbol_lookup_name(symbols, lbl, &val);
			} else {
				val = parse_value(line, NULL);
				while (*line && !isspace((unsigned char)*line) &&
				       *line != ',' && *line != ';') line++;
			}
			if (mem) {
				mem->mem[(*pc)++] = val & 0xFF;
				mem->mem[(*pc)++] = (val >> 8) & 0xFF;
			} else {
				(*pc) += 2;
			}
			while (*line && isspace((unsigned char)*line)) line++;
			if (*line == ',') line++;
		}
		return;
	}
	if (strncmp(line, "text", 4) == 0 && (!line[4] || isspace(line[4]))) {
		line += 4;
		while (*line && isspace(*line)) line++;
		if (*line == '"') {
			line++;
			while (*line && *line != '"') {
				unsigned char c;
				if (*line == '\\') {
					line++;
					switch (*line) {
					case 'n':  c = '\n'; break;
					case 'r':  c = '\r'; break;
					case 't':  c = '\t'; break;
					case '0':  c = '\0'; break;
					case '\\': c = '\\'; break;
					case '"':  c = '"';  break;
					default:   c = (unsigned char)*line; break;
					}
				} else {
					c = (unsigned char)*line;
				}
				line++;
				if (mem) mem->mem[(*pc)++] = c;
				else (*pc)++;
			}
		}
		return;
	}
	if (strncmp(line, "align", 5) == 0 && (!line[5] || isspace(line[5]))) {
		line += 5;
		while (*line && isspace(*line)) line++;
		int n = (int)parse_value(line, NULL);
		if (n > 1) {
			int rem = (*pc) % n;
			if (rem != 0) {
				int pad = n - rem;
				if (mem) {
					for (int i = 0; i < pad; i++) mem->mem[(*pc)++] = 0;
				} else {
					*pc += pad;
				}
			}
		}
		return;
	}
	if (strncmp(line, "bin", 3) == 0 && (!line[3] || isspace(line[3]))) {
		line += 3;
		while (*line && isspace(*line)) line++;
		if (*line == '"') {
			line++;
			char fname[256]; int j = 0;
			while (*line && *line != '"' && j < 255) fname[j++] = *line++;
			fname[j] = 0;
			int n = load_binary_to_mem(mem, *pc, fname);
			if (n > 0) *pc += n;
		}
		return;
	}
}

void parse_line(const char *line, instruction_t *instr, symbol_table_t *symbols, int pc) {
	int i = 0;
	const char *semi  = strchr(line, ';');
	const char *colon = strchr(line, ':');
	if (colon && (!semi || colon < semi)) line = colon + 1;
	while (*line && isspace(*line)) line++;
	if (!*line || *line == ';' || *line == '.') {
		instr->op[0] = 0;
		return;
	}
	while (*line && !isspace(*line) && i < (int)sizeof(instr->op) - 1)
		instr->op[i++] = toupper(*line++);
	instr->op[i] = 0;
	while (*line && isspace(*line)) line++;
	instr->mode = MODE_IMPLIED;
	instr->arg = 0;
	instr->arg_overflow = 0;
	int is_branch = is_branch_opcode(instr->op);
	if (*line == '#') {
		if (strcmp(instr->op, "PHW") == 0) instr->mode = MODE_IMMEDIATE_WORD;
		else instr->mode = MODE_IMMEDIATE;
		line++;
		instr->arg = parse_value(line, NULL);
		/* immediate: value must fit in one byte (or two bytes for PHW) */
		if (instr->mode == MODE_IMMEDIATE && instr->arg > 0xFF)
			instr->arg_overflow = 1;
	} else if (*line == '$' || *line == '%' || *line == '\'' || isdigit(*line) || *line == '*') {
		int digits = 4;
		if (*line == '*') {
			instr->arg = (unsigned short)pc;
			line++;
			if (*line == '+') {
				line++;
				instr->arg += parse_value(line, NULL);
			} else if (*line == '-') {
				line++;
				instr->arg -= parse_value(line, NULL);
			}
		} else {
			instr->arg = parse_value(line, &digits);
		}
		while (*line && !isspace(*line) && *line != ',') line++;
		while (*line && (isspace(*line) || *line == ',')) line++;
		if (toupper(*line) == 'X') instr->mode = (digits <= 2) ? MODE_ZP_X : MODE_ABSOLUTE_X;
		else if (toupper(*line) == 'Y') instr->mode = (digits <= 2) ? MODE_ZP_Y : MODE_ABSOLUTE_Y;
		else if (is_branch) {
			unsigned short target = instr->arg;
			if (is_long_branch_opcode(instr->op)) {
				instr->mode = MODE_RELATIVE_LONG;
				instr->arg = (unsigned short)(target - (pc + 3));
			} else {
				instr->mode = MODE_RELATIVE;
				instr->arg = (unsigned short)(target - (pc + 2));
			}
		} else instr->mode = (digits <= 2) ? MODE_ZP : MODE_ABSOLUTE;
		/* hex literals only: >4 digits means value exceeds 16-bit address space */
		switch (instr->mode) {
		case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
			if (digits > 4) instr->arg_overflow = 1;
			break;
		default:
			break;
		}
	} else if (*line == '(') {
		line++;
		if (*line == '$' || *line == '%' || *line == '\'' || isdigit(*line)) {
			instr->arg = parse_value(line, NULL);
			while (*line && *line != ')' && *line != ',') line++;
			if (*line == ',') {
				line++;
				while (*line && isspace(*line)) line++;
				if (toupper(*line) == 'X') {
					if (instr->arg > 0xFF) instr->mode = MODE_ABS_INDIRECT_X;
					else instr->mode = MODE_INDIRECT_X;
					while (*line && *line != ')') line++;
					if (*line == ')') line++;
				} else if (toupper(*line) == 'S') {
					if (toupper(line[1]) == 'P') line += 2;
					else line++;
					while (*line && isspace(*line)) line++;
					if (*line == ')') {
						line++;
						while (*line && isspace(*line)) line++;
						if (*line == ',') {
							line++;
							while (*line && isspace(*line)) line++;
							if (toupper(*line) == 'Y') instr->mode = MODE_SP_INDIRECT_Y;
						}
					}
				}
			} else if (*line == ')') {
				line++;
				while (*line && isspace(*line)) line++;
				if (*line == ',') {
					line++;
					while (*line && isspace(*line)) line++;
					if (toupper(*line) == 'Y') instr->mode = (instr->arg > 0xFF) ? MODE_ABS_INDIRECT_Y : MODE_INDIRECT_Y;
					else if (toupper(*line) == 'Z') instr->mode = MODE_ZP_INDIRECT_Z;
				} else {
					if (instr->arg > 0xFF) instr->mode = MODE_INDIRECT;
					else instr->mode = MODE_ZP_INDIRECT;
				}
			}
		} else if (isalpha(*line) || *line == '_') {
			/* Symbol name inside parens: look up its value, then detect mode */
			char lbl[64]; int j = 0;
			while ((*line && (isalnum(*line) || *line == '_')) && j < 63) lbl[j++] = *line++;
			lbl[j] = 0;
			instr->arg = 0;
			if (symbols) symbol_lookup_name(symbols, lbl, &instr->arg);
			while (*line && isspace(*line)) line++;
			if (*line == ',') {
				line++;
				while (*line && isspace(*line)) line++;
				if (toupper(*line) == 'X') {
					if (instr->arg > 0xFF) instr->mode = MODE_ABS_INDIRECT_X;
					else instr->mode = MODE_INDIRECT_X;
					while (*line && *line != ')') line++;
					if (*line == ')') line++;
				} else if (toupper(*line) == 'S') {
					if (toupper(line[1]) == 'P') line += 2;
					else line++;
					while (*line && isspace(*line)) line++;
					if (*line == ')') {
						line++;
						while (*line && isspace(*line)) line++;
						if (*line == ',') {
							line++;
							while (*line && isspace(*line)) line++;
							if (toupper(*line) == 'Y') instr->mode = MODE_SP_INDIRECT_Y;
						}
					}
				}
			} else if (*line == ')') {
				line++;
				while (*line && isspace(*line)) line++;
				if (*line == ',') {
					line++;
					while (*line && isspace(*line)) line++;
					if (toupper(*line) == 'Y') instr->mode = (instr->arg > 0xFF) ? MODE_ABS_INDIRECT_Y : MODE_INDIRECT_Y;
					else if (toupper(*line) == 'Z') instr->mode = MODE_ZP_INDIRECT_Z;
				} else {
					if (instr->arg > 0xFF) instr->mode = MODE_INDIRECT;
					else instr->mode = MODE_ZP_INDIRECT;
				}
			}
		}
	} else if (*line == '[') {
		line++;
		if (*line == '$' || *line == '%' || *line == '\'' || isdigit(*line)) {
			instr->arg = parse_value(line, NULL);
			while (*line && *line != ']') line++;
			if (*line == ']') line++;
			while (*line && isspace(*line)) line++;
			if (*line == ',') {
				line++;
				while (*line && isspace(*line)) line++;
				if (toupper(*line) == 'Z')
					instr->mode = MODE_ZP_INDIRECT_Z_FLAT;
			} else {
				instr->mode = MODE_ZP_INDIRECT_FLAT;
			}
		}
	} else if (instr->op[0]) {
		if (isalpha(*line) || *line == '_' || *line == '.') {
			char label[64];
			int j = 0;
			if (*line == '.' && j < 63) label[j++] = *line++;  /* allow leading dot */
			while ((*line && (isalnum(*line) || *line == '_')) && j < 63) label[j++] = *line++;
			label[j] = 0;
			if (is_branch) {
				if (is_long_branch_opcode(instr->op))
					instr->mode = MODE_RELATIVE_LONG;
				else
					instr->mode = MODE_RELATIVE;
			} else instr->mode = MODE_ABSOLUTE;
			if (symbols) {
				unsigned short addr;
				if (symbol_lookup_name(symbols, label, &addr)) {
					if (instr->mode == MODE_RELATIVE) instr->arg = (unsigned short)(addr - (pc + 2));
					else if (instr->mode == MODE_RELATIVE_LONG) instr->arg = (unsigned short)(addr - (pc + 3));
					else instr->arg = addr;
				} else {
					fprintf(stderr, "Warning: undefined label '%s'\n", label);
					instr->arg = 0;
				}
			}
		} else instr->mode = MODE_IMPLIED;
	}
}

int encode_to_mem(memory_t *mem, int pc_base,
		const instruction_t *instr,
		const opcode_handler_t *handlers, int n,
		cpu_type_t cpu_type) {
	if (instr->arg_overflow) return -1;
	if (strcmp(instr->op, "BRK") == 0) {
		mem->mem[pc_base] = 0x00;
		if (cpu_type != CPU_45GS02) {
			mem->mem[pc_base + 1] = 0x00;
			return 2;
		}
		return 1;
	}
	int pc = pc_base;
	int i;
	for (i = 0; i < n; i++) {
		if (strcmp(instr->op, handlers[i].mnemonic) == 0 &&
		    handlers[i].mode == instr->mode &&
		    handlers[i].opcode_len > 0)
			break;
	}
	instruction_t adjusted_instr = *instr;
	if (i >= n && (instr->mode == MODE_RELATIVE || instr->mode == MODE_RELATIVE_LONG)) {
		/* Fallback: try the other relative mode if this one didn't match.
		 * This handles mnemonics like BRL/BSR that might be short on some CPUs and long on others. */
		unsigned char other_mode = (instr->mode == MODE_RELATIVE) ? MODE_RELATIVE_LONG : MODE_RELATIVE;
		for (i = 0; i < n; i++) {
			if (strcmp(instr->op, handlers[i].mnemonic) == 0 &&
			    handlers[i].mode == other_mode &&
			    handlers[i].opcode_len > 0) {
				/* Found a match in the other mode. Recalculate target address from original arg,
				 * then recalculate relative offset for the new mode.
				 * Original arg was (target - (pc_base + len_of_orig_mode)) */
				int orig_len = (instr->mode == MODE_RELATIVE) ? 2 : 3;
				int new_len = (other_mode == MODE_RELATIVE) ? 2 : 3;
				unsigned short target = (unsigned short)(pc_base + orig_len + (short)instr->arg);
				adjusted_instr.mode = other_mode;
				adjusted_instr.arg = (unsigned short)(target - (pc_base + new_len));
				break;
			}
		}
	}
	if (i >= n) return -1;
	const instruction_t *final_instr = (i < n && adjusted_instr.mode != instr->mode) ? &adjusted_instr : instr;

	unsigned char olen = handlers[i].opcode_len;
	for (int j = 0; j < olen; j++)
		mem->mem[pc++] = handlers[i].opcode_bytes[j];
	switch (handlers[i].mode) {
	case MODE_IMPLIED:
		break;
	case MODE_IMMEDIATE: case MODE_ZP: case MODE_ZP_X: case MODE_ZP_Y:
	case MODE_INDIRECT_X: case MODE_INDIRECT_Y:
	case MODE_ZP_INDIRECT: case MODE_ZP_INDIRECT_Z:
	case MODE_ZP_INDIRECT_Z_FLAT: case MODE_ZP_INDIRECT_FLAT:
	case MODE_SP_INDIRECT_Y: case MODE_RELATIVE:
		mem->mem[pc++] = final_instr->arg & 0xFF;
		break;
	case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
	case MODE_INDIRECT: case MODE_ABS_INDIRECT_X: case MODE_ABS_INDIRECT_Y:
	case MODE_RELATIVE_LONG: case MODE_IMMEDIATE_WORD:
		mem->mem[pc++] = final_instr->arg & 0xFF;
		mem->mem[pc++] = (final_instr->arg >> 8) & 0xFF;
		break;
	}
	return pc - pc_base;
}
