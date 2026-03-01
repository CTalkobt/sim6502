#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "cycles.h"
#include "breakpoint.h"
#include "trace.h"
#include "memory_viewer.h"
#include "interrupt_manager.h"
#include "symbol_table.h"

/* 
 * instruction_t: Represents a parsed instruction.
 * Global ROM array holds the program to allow random access (loops).
 */
typedef struct {
	char op[8];
	unsigned char mode;
	unsigned short arg;
} instruction_t;

static instruction_t rom[65536]; /* 64KB Instruction ROM */

static const char *processor_name(cpu_type_t type) {
	switch (type) {
	case CPU_6502: return "6502";
	case CPU_6502_UNDOCUMENTED: return "6502 (undocumented)";
	case CPU_65C02: return "65C02";
	case CPU_65CE02: return "65CE02";
	case CPU_45GS02: return "45GS02";
	default: return "unknown";
	}
}

static const char *mode_name(unsigned char mode) {
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

/* Return the total encoded byte count for (mnemonic,mode), including
 * any multi-byte prefix (e.g. $42 $42 for quad, $EA for flat).
 * Falls back to get_instruction_length if not found. */
static int get_encoded_length(const char *mnem, unsigned char mode,
		const opcode_handler_t *handlers, int n) {
	for (int i = 0; i < n; i++) {
		if (strcmp(handlers[i].mnemonic, mnem) == 0 &&
		    handlers[i].mode == mode &&
		    handlers[i].opcode_len > 0) {
			/* operand bytes = get_instruction_length(mode) - 1 */
			int operand = 0;
			switch (mode) {
			case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
			case MODE_INDIRECT: case MODE_ABS_INDIRECT_X:
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
	return -1; /* not found */
}

/* Helper to determine instruction length based on addressing mode */
static int get_instruction_length(unsigned char mode) {
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

/* Read the instruction operand from mem[cpu->pc+1..] based on addressing mode.
 * Does NOT modify PC. */
static unsigned short decode_operand(cpu_t *cpu, memory_t *mem, unsigned char mode) {
	switch (mode) {
	case MODE_IMPLIED:
		return 0;
	case MODE_IMMEDIATE: case MODE_ZP: case MODE_ZP_X: case MODE_ZP_Y:
	case MODE_INDIRECT_X: case MODE_INDIRECT_Y:
	case MODE_ZP_INDIRECT: case MODE_ZP_INDIRECT_Z:
	case MODE_ZP_INDIRECT_FLAT: case MODE_ZP_INDIRECT_Z_FLAT:
	case MODE_SP_INDIRECT_Y: case MODE_RELATIVE: case MODE_ABS_INDIRECT_Y:
		return mem_read(mem, (unsigned short)(cpu->pc + 1));
	case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
	case MODE_INDIRECT: case MODE_ABS_INDIRECT_X:
	case MODE_RELATIVE_LONG: case MODE_IMMEDIATE_WORD:
		return (unsigned short)(mem_read(mem, (unsigned short)(cpu->pc + 1)) |
		        (mem_read(mem, (unsigned short)(cpu->pc + 2)) << 8));
	default:
		return 0;
	}
}

static int is_branch_opcode(const char *op) {
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
	if (strcmp(op, "BRL") == 0) return 1;
	return 0;
}

static int is_long_branch_opcode(const char *op) {
	if (op[0] == 'L' && (strcmp(op, "LBRL") != 0)) {
		/* Most L-prefixed branches are long on 45GS02 */
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

static void print_help(const char *progname) {
	printf("6502 Simulator - Professional 6502 Development & Debugging Platform\n");
	printf("==================================================================\n\n");
	printf("Usage: %s [options] <file.asm>\n\n", progname);
	
	printf("DESCRIPTION:\n");
	printf("  A feature-rich simulator for 6502 and compatible processors with\n");
	printf("  professional debugging tools, symbol table support, and interrupts.\n\n");

	printf("PROCESSOR SELECTION:\n");
	printf("  -p, --processor <CPU>  Select processor: 6502, 6502-undoc, 65c02, 65ce02, 45gs02\n");
	printf("  -l, --list             List available processors\n");
	printf("  -o, --opcodes <CPU>    List opcodes for processor\n");
	printf("  --info <OPCODE>        Show detailed info for opcode (uses current processor)\n\n");

	printf("DEBUGGING:\n");
	printf("  -I, --interactive      Start in interactive debug mode\n");
	printf("  -b, --break <ADDR>     Set breakpoint (hex address, e.g., 0x1000)\n");
	printf("  -t, --trace [FILE]     Enable execution trace (optional: output to FILE)\n");
	printf("  -a, --address <ADDR>   Set start address (hex or label)\n\n");

	printf("MEMORY:\n");
	printf("  -m, --mem <START:END>  View memory range on exit (e.g., 0x1000:0x1100)\n");
	printf("  -s, --stats            Show memory statistics on exit\n\n");

	printf("SYMBOL TABLES:\n");
	printf("  --symbols <FILE>       Load custom symbol table from FILE\n");
	printf("  --preset <ARCH>        Load preset symbols: c64, c128, mega65, x16\n");
	printf("  --show-symbols         Display loaded symbol table on exit\n\n");

	printf("INTERRUPTS:\n");
	printf("  -i, --irq <CYCLES>     Trigger IRQ at cycle count\n");
	printf("  -n, --nmi <CYCLES>     Trigger NMI at cycle count\n\n");

	printf("OTHER:\n");
	printf("  -h, --help             Show this help message\n\n");

	printf("EXAMPLES:\n");
	printf("  %s program.asm                        Basic execution\n", progname);
	printf("  %s -I program.asm                     Interactive debug session\n", progname);
	printf("  %s --info LDA                         Show LDA details\n", progname);
}

static void list_processors(void) {
	printf("Available Processors:\n\n");
	printf("  6502            - Original (151 instructions)\n");
	printf("  6502-undoc      - With undocumented (169 instructions)\n");
	printf("  65c02           - Enhanced (178 instructions)\n");
	printf("  65ce02          - Embedded variant (200+ instructions)\n");
	printf("  45gs02          - Mega65 (240+ instructions)\n\n");
}

static void list_opcodes(cpu_type_t type) {
	opcode_handler_t *handlers = NULL;
	int num_handlers = 0;
	
	if (type == CPU_6502_UNDOCUMENTED) {
		handlers = opcodes_6502_undoc;
		num_handlers = OPCODES_6502_UNDOC_COUNT;
	} else if (type == CPU_65C02) {
		handlers = opcodes_65c02;
		num_handlers = OPCODES_65C02_COUNT;
	} else if (type == CPU_65CE02) {
		handlers = opcodes_65ce02;
		num_handlers = OPCODES_65CE02_COUNT;
	} else if (type == CPU_45GS02) {
		handlers = opcodes_45gs02;
		num_handlers = OPCODES_45GS02_COUNT;
	} else {
		handlers = opcodes_6502;
		num_handlers = OPCODES_6502_COUNT;
	}
	
	printf("Opcodes for %s (%d total):\n\n", processor_name(type), num_handlers);
	for (int i = 0; i < num_handlers; i += 3) {
		for (int j = 0; j < 3 && i + j < num_handlers; j++) {
			printf("%-20s", handlers[i+j].mnemonic);
		}
		printf("\n");
	}
	printf("\n");
}

static void print_opcode_info(opcode_handler_t *handlers, int num_handlers, const char *mnemonic) {
	char upper_mnemonic[16];
	int i;
	for (i = 0; i < 15 && mnemonic[i]; i++)
		upper_mnemonic[i] = toupper(mnemonic[i]);
	upper_mnemonic[i] = 0;

	printf("Opcode Information for %s:\n\n", upper_mnemonic);
	printf("%-15s %-10s %-10s %-10s\n", "Mode", "Byte", "Length", "Cycles");
	printf("---------------------------------------------------\n");
	
	int found = 0;
	for (i = 0; i < num_handlers; i++) {
		if (strcmp(handlers[i].mnemonic, upper_mnemonic) == 0) {
			int len = get_instruction_length(handlers[i].mode);
			/* Special cases for length */
			if (strcmp(handlers[i].mnemonic, "BRK") == 0) len = 2;
			if (handlers[i].mode == MODE_RELATIVE) len = 2;
			if (handlers[i].mode == MODE_RELATIVE_LONG) len = 3;
			if (strncmp(handlers[i].mnemonic, "BBR", 3) == 0 || strncmp(handlers[i].mnemonic, "BBS", 3) == 0) len = 3;
			
			/* Build a display value from opcode_bytes for printing */
			unsigned int opcode = 0;
			for (int ob = 0; ob < handlers[i].opcode_len; ob++)
				opcode = (opcode << 8) | handlers[i].opcode_bytes[ob];
			int cycles = handlers[i].cycles_6502;

			printf("%-15s $%-9X %-10d %-10d\n", 
				mode_name(handlers[i].mode), opcode, len, cycles);
			found = 1;
		}
	}
	
	if (!found) {
		printf("Opcode '%s' not found.\n", upper_mnemonic);
	}
	printf("\n");
}

static void parse_pseudo_op(const char *line, cpu_type_t *cpu_type) {
	if (strncmp(line, ".processor", 10) == 0) {
		line += 10;
		while (*line && isspace(*line)) line++;
		
		if (strncmp(line, "45gs02", 6) == 0 || strncmp(line, "45GS02", 6) == 0) {
			*cpu_type = CPU_45GS02;
		} else if (strncmp(line, "65ce02", 6) == 0 || strncmp(line, "65CE02", 6) == 0) {
			*cpu_type = CPU_65CE02;
		} else if (strncmp(line, "65c02", 5) == 0 || strncmp(line, "65C02", 5) == 0) {
			*cpu_type = CPU_65C02;
		} else if (strncmp(line, "6502", 4) == 0) {
			line += 4;
			while (*line && isspace(*line)) line++;
			if (strncmp(line, "undoc", 5) == 0)
				*cpu_type = CPU_6502_UNDOCUMENTED;
			else
				*cpu_type = CPU_6502;
		}
	}
}

/* parse_value: convert a literal constant to a numeric value.
 * Supported formats:
 *   $xx / $xxxx  — hexadecimal
 *   %xxxxxxxx    — binary
 *   'c'          — ASCII character constant
 *   123          — decimal
 * If out_digits is non-NULL, sets it to a digit-count hint used to decide
 * between ZP and absolute addressing: <=2 means byte-sized, >2 means word. */
static unsigned short parse_value(const char *str, int *out_digits) {
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

static void parse_line(const char *line, instruction_t *instr, symbol_table_t *symbols, int pc) {
	int i = 0;
	const char *colon = strchr(line, ':');
	if (colon) line = colon + 1;

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

	int is_branch = is_branch_opcode(instr->op);

	if (*line == '#') {
		if (strcmp(instr->op, "PHW") == 0) instr->mode = MODE_IMMEDIATE_WORD;
		else instr->mode = MODE_IMMEDIATE;
		line++;
		instr->arg = parse_value(line, NULL);
	} else if (*line == '$' || *line == '%' || *line == '\'' || isdigit(*line)) {
		int digits;
		instr->arg = parse_value(line, &digits);

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
					/* SP-relative indirect Y: ($nn,SP),Y */
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
					if (toupper(*line) == 'Y') instr->mode = MODE_INDIRECT_Y;
					else if (toupper(*line) == 'Z') instr->mode = MODE_ZP_INDIRECT_Z;
				} else {
					if (instr->arg > 0xFF) instr->mode = MODE_INDIRECT;
					else instr->mode = MODE_ZP_INDIRECT;
				}
			}
		}
	} else if (*line == '[') {
		/* [zp] or [zp],Z — flat/far indirect via 32-bit ZP pointer */
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
				/* [zp] without ,Z — flat indirect, no Z offset */
				instr->mode = MODE_ZP_INDIRECT_FLAT;
			}
		}
	} else if (instr->op[0]) {
		if (isalpha(*line) || *line == '_') {
			char label[64];
			int j = 0;
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

/* handle_trap: if cpu->pc matches a TRAP symbol, print a register dump and
 * simulate RTS (pop return address, advance PC).
 * Returns:  0 = no trap at this address
 *           1 = trap fired, continue execution
 *          -1 = trap fired but return address is $0000 (uninitialized stack /
 *               reached via JMP not JSR) — caller should stop execution */
static int handle_trap(const symbol_table_t *st, cpu_t *cpu, memory_t *mem,
		const opcode_handler_t *handlers) {
	for (int i = 0; i < st->count; i++) {
		if (st->symbols[i].type != SYM_TRAP) continue;
		if (st->symbols[i].address != cpu->pc) continue;

		int is_ext = (handlers == opcodes_45gs02 || handlers == opcodes_65ce02);
		printf("[TRAP] %-20s $%04X  A=%02X X=%02X Y=%02X",
			st->symbols[i].name, cpu->pc,
			cpu->a, cpu->x, cpu->y);
		if (is_ext)
			printf(" Z=%02X B=%02X", cpu->z, cpu->b);
		printf(" S=%02X P=%02X", cpu->s, cpu->p);
		if (st->symbols[i].comment[0])
			printf("  ; %s", st->symbols[i].comment);
		printf("\n");

		/* Nominal cycle cost — prevents infinite-trap spin when a trap is
		 * inside a loop (without this, cycles never advance past the limit). */
		cpu->cycles += 6;

		/* Simulate RTS: pop the return address pushed by the caller's JSR */
		cpu->s++;
		unsigned short lo = mem_read(mem, 0x100 + cpu->s);
		cpu->s++;
		unsigned short hi = mem_read(mem, 0x100 + cpu->s);
		unsigned short ret = (unsigned short)(((unsigned short)hi << 8) | lo);
		ret++;	/* RTS convention: stored as addr-1 */

		/* Guard: if the return address is $0000 the stack was uninitialized
		 * (trap reached via JMP/fallthrough, not JSR) or the NMI/IRQ/BRK
		 * vector is zeroed out.  Branch to $0000 would silently re-execute
		 * the whole program, so stop here instead. */
		if (ret == 0) {
			printf("[TRAP] return address is $0000 — halting (stack was empty or vector uninitialised)\n");
			cpu->pc = ret;
			return -1;
		}

		cpu->pc = ret;
		return 1;
	}
	return 0;
}

static void execute(cpu_t *cpu, memory_t *mem, instruction_t *instr,
		const opcode_handler_t *handlers, int num_handlers) {
	if (!instr->op[0]) return;
	/* Promote a fresh EOM signal (1) to the active flat sentinel (2) that
	 * _zp_ind_z handlers check.  Any stale sentinel (2) is cleared to 0. */
	cpu->eom_prefix = (cpu->eom_prefix == 1) ? 2 : 0;
	for (int i = 0; i < num_handlers; i++) {
		if (strcmp(instr->op, handlers[i].mnemonic) == 0 && handlers[i].mode == instr->mode) {
			handlers[i].fn(cpu, mem, instr->arg);
			return;
		}
	}
	cpu->eom_prefix = 0;
}

/* ============================================================
 * Dispatch table — O(1) byte-indexed execution (Phases 2+4)
 * ============================================================ */
typedef struct {
	opcode_fn     fn;
	unsigned char mode;
	const char   *mnemonic;
	unsigned char cycles;
} dispatch_entry_t;

typedef struct {
	dispatch_entry_t base[256];     /* indexed by single opcode byte */
	dispatch_entry_t quad[256];     /* 45GS02: $42 $42 prefix, indexed by 3rd byte */
	dispatch_entry_t quad_eom[256]; /* 45GS02: $42 $42 $EA prefix, indexed by 4th byte */
} dispatch_table_t;

static void dispatch_build(dispatch_table_t *dt,
		const opcode_handler_t *handlers, int n, cpu_type_t cpu_type) {
	memset(dt, 0, sizeof(*dt));
	for (int i = 0; i < n; i++) {
		unsigned char olen = handlers[i].opcode_len;
		if (olen == 0) continue;  /* unassigned sentinel */
		unsigned char cyc;
		switch (cpu_type) {
		case CPU_65C02:  cyc = handlers[i].cycles_65c02  ? handlers[i].cycles_65c02  : handlers[i].cycles_6502; break;
		case CPU_65CE02: cyc = handlers[i].cycles_65ce02 ? handlers[i].cycles_65ce02 : handlers[i].cycles_6502; break;
		case CPU_45GS02: cyc = handlers[i].cycles_45gs02 ? handlers[i].cycles_45gs02 : handlers[i].cycles_6502; break;
		default:         cyc = handlers[i].cycles_6502; break;
		}

		dispatch_entry_t *slot = NULL;
		unsigned char key = handlers[i].opcode_bytes[olen - 1];

		if (olen == 1) {
			/* Single-byte opcode → base table */
			slot = &dt->base[key];
		} else if (olen == 2 && handlers[i].opcode_bytes[0] == 0xEA) {
			/* EOM ($EA) + base opcode: handled by EOM chaining in execute_from_mem;
			 * do NOT put in quad[] — EOM in base[0xEA] sets eom_prefix, then the
			 * base opcode byte is dispatched normally on the next call. Skip here. */
			continue;
		} else if (olen == 3 && handlers[i].opcode_bytes[0] == 0x42 &&
		           handlers[i].opcode_bytes[1] == 0x42) {
			/* $42 $42 + base opcode → quad table */
			slot = &dt->quad[key];
		} else if (olen == 4 && handlers[i].opcode_bytes[0] == 0x42 &&
		           handlers[i].opcode_bytes[1] == 0x42 &&
		           handlers[i].opcode_bytes[2] == 0xEA) {
			/* $42 $42 $EA + base opcode → quad_eom table */
			slot = &dt->quad_eom[key];
		}
		/* Any other pattern is unrecognised — skip */

		if (slot && !slot->fn) {
			slot->fn       = handlers[i].fn;
			slot->mode     = handlers[i].mode;
			slot->mnemonic = handlers[i].mnemonic;
			slot->cycles   = cyc;
		}
	}
}

/* Return the dispatch entry for the instruction at cpu->pc without executing.
 * Peeks past $42 $42 and $42 $42 $EA prefixes for 45GS02. */
static const dispatch_entry_t *peek_dispatch(const cpu_t *cpu, const memory_t *mem,
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

/* Fetch opcode from mem[], dispatch via table, execute.
 * Handles 45GS02 NEG NEG ($42 $42) quad prefix and $42 $42 $EA quad-flat. */
static void execute_from_mem(cpu_t *cpu, memory_t *mem,
		const dispatch_table_t *dt, cpu_type_t cpu_type) {
	unsigned char byte0 = mem_read(mem, cpu->pc);
	if (cpu_type == CPU_45GS02 && byte0 == 0x42) {
		unsigned char byte1 = mem_read(mem, (unsigned short)(cpu->pc + 1));
		if (byte1 == 0x42) {
			unsigned char byte2 = mem_read(mem, (unsigned short)(cpu->pc + 2));
			if (byte2 == 0xEA) {
				/* $42 $42 $EA $base — quad flat indirect */
				unsigned char base = mem_read(mem, (unsigned short)(cpu->pc + 3));
				const dispatch_entry_t *e = &dt->quad_eom[base];
				if (e->fn) {
					cpu->pc += 3;  /* consume $42 $42 $EA; handler at base opcode */
					cpu->eom_prefix = 2;  /* flat is active */
					unsigned short arg = decode_operand(cpu, mem, e->mode);
					e->fn(cpu, mem, arg);
					return;
				}
			} else {
				/* $42 $42 $base — quad non-flat */
				const dispatch_entry_t *e = &dt->quad[byte2];
				if (e->fn) {
					cpu->pc += 2;  /* consume $42 $42; handler sees PC at base opcode */
					cpu->eom_prefix = (cpu->eom_prefix == 1) ? 2 : 0;
					unsigned short arg = decode_operand(cpu, mem, e->mode);
					e->fn(cpu, mem, arg);
					return;
				}
			}
		}
	}
	/* Normal single-byte dispatch (also handles EOM via eom_prefix chaining) */
	cpu->eom_prefix = (cpu->eom_prefix == 1) ? 2 : 0;
	const dispatch_entry_t *e = &dt->base[byte0];
	if (!e->fn) { cpu->pc++; return; }  /* unimplemented: skip */
	unsigned short arg = decode_operand(cpu, mem, e->mode);
	e->fn(cpu, mem, arg);
}

/* Encode one instruction into mem[].
 * pc_base is the address to write at.
 * Returns the total number of bytes written, or -1 if not found. */
static int encode_to_mem(memory_t *mem, int pc_base,
		const instruction_t *instr,
		const opcode_handler_t *handlers, int n) {
	int pc = pc_base;

	/* Find handler by (mnemonic, mode) with an assigned opcode */
	int i;
	for (i = 0; i < n; i++) {
		if (strcmp(instr->op, handlers[i].mnemonic) == 0 &&
		    handlers[i].mode == instr->mode &&
		    handlers[i].opcode_len > 0)
			break;
	}
	if (i >= n) return -1;  /* not found — leave memory zeroed */

	/* Write opcode bytes in order (first to last) */
	unsigned char olen = handlers[i].opcode_len;
	for (int j = 0; j < olen; j++)
		mem->mem[pc++] = handlers[i].opcode_bytes[j];

	/* Write operand byte(s) */
	switch (handlers[i].mode) {
	case MODE_IMPLIED:
		break;
	case MODE_IMMEDIATE: case MODE_ZP: case MODE_ZP_X: case MODE_ZP_Y:
	case MODE_INDIRECT_X: case MODE_INDIRECT_Y:
	case MODE_ZP_INDIRECT: case MODE_ZP_INDIRECT_Z:
	case MODE_ZP_INDIRECT_Z_FLAT: case MODE_ZP_INDIRECT_FLAT:
	case MODE_SP_INDIRECT_Y: case MODE_RELATIVE: case MODE_ABS_INDIRECT_Y:
		mem->mem[pc++] = instr->arg & 0xFF;
		break;
	case MODE_ABSOLUTE: case MODE_ABSOLUTE_X: case MODE_ABSOLUTE_Y:
	case MODE_INDIRECT: case MODE_ABS_INDIRECT_X:
	case MODE_RELATIVE_LONG: case MODE_IMMEDIATE_WORD:
		mem->mem[pc++] = instr->arg & 0xFF;
		mem->mem[pc++] = (instr->arg >> 8) & 0xFF;
		break;
	}
	return pc - pc_base;  /* total bytes written */
}

static void run_interactive_mode(cpu_t *cpu, memory_t *mem, instruction_t *rom,
                                 opcode_handler_t **p_handlers, int *p_num_handlers,
                                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                                 unsigned short start_addr, breakpoint_list_t *breakpoints,
                                 symbol_table_t *symbols) {
    char line[256];
    char cmd[32];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("6502 Simulator Interactive Mode\nType 'help' for commands.\n");
    
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        if (sscanf(line, "%31s", cmd) != 1) continue;
        
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "help") == 0) {
            printf("Commands: step [n], run, break <addr>, clear <addr>, list, regs, mem <addr> <len>, write <addr> <val>, reset, processors, processor <type>, info <opcode>, quit\n");
            printf("          jump <addr>, set <reg> <val>, flag <flag> <0|1>\n");
        } else if (strcmp(cmd, "break") == 0) {
            unsigned int addr;
            if (sscanf(line, "%*s %x", &addr) == 1) breakpoint_add(breakpoints, (unsigned short)addr);
        } else if (strcmp(cmd, "clear") == 0) {
            unsigned int addr;
            if (sscanf(line, "%*s %x", &addr) == 1) breakpoint_remove(breakpoints, (unsigned short)addr);
        } else if (strcmp(cmd, "list") == 0) breakpoint_list(breakpoints);
        else if (strcmp(cmd, "jump") == 0) {
            unsigned int addr;
            if (sscanf(line, "%*s %x", &addr) == 1) {
                cpu->pc = (unsigned short)addr;
                printf("PC set to %04X\n", cpu->pc);
            } else {
                printf("Usage: jump <addr>\n");
            }
        } else if (strcmp(cmd, "set") == 0) {
            char reg[16];
            unsigned int val;
            if (sscanf(line, "%*s %15s %x", reg, &val) == 2) {
                if (strcmp(reg, "A") == 0 || strcmp(reg, "a") == 0) cpu->a = (unsigned char)val;
                else if (strcmp(reg, "X") == 0 || strcmp(reg, "x") == 0) cpu->x = (unsigned char)val;
                else if (strcmp(reg, "Y") == 0 || strcmp(reg, "y") == 0) cpu->y = (unsigned char)val;
                else if (strcmp(reg, "Z") == 0 || strcmp(reg, "z") == 0) cpu->z = (unsigned char)val;
                else if (strcmp(reg, "B") == 0 || strcmp(reg, "b") == 0) cpu->b = (unsigned char)val;
                else if (strcmp(reg, "S") == 0 || strcmp(reg, "s") == 0 || strcmp(reg, "SP") == 0 || strcmp(reg, "sp") == 0) cpu->s = (unsigned short)val;
                else if (strcmp(reg, "P") == 0 || strcmp(reg, "p") == 0) cpu->p = (unsigned char)val;
                else if (strcmp(reg, "PC") == 0 || strcmp(reg, "pc") == 0) cpu->pc = (unsigned short)val;
                else printf("Unknown register: %s\n", reg);
                printf("Register %s set to %02X\n", reg, val);
            } else {
                printf("Usage: set <reg> <val>\n");
            }
        } else if (strcmp(cmd, "flag") == 0) {
            char flag_name[16];
            int val;
            if (sscanf(line, "%*s %15s %d", flag_name, &val) == 2) {
                unsigned char flag_mask = 0;
                if (toupper(flag_name[0]) == 'C') flag_mask = FLAG_C;
                else if (toupper(flag_name[0]) == 'Z') flag_mask = FLAG_Z;
                else if (toupper(flag_name[0]) == 'I') flag_mask = FLAG_I;
                else if (toupper(flag_name[0]) == 'D') flag_mask = FLAG_D;
                else if (toupper(flag_name[0]) == 'B') flag_mask = FLAG_B;
                else if (toupper(flag_name[0]) == 'V') flag_mask = FLAG_V;
                else if (toupper(flag_name[0]) == 'N') flag_mask = FLAG_N;
                
                if (flag_mask) {
                    if (val) cpu->p |= flag_mask;
                    else cpu->p &= ~flag_mask;
                    printf("Flag %c set to %d (P=%02X)\n", toupper(flag_name[0]), val ? 1 : 0, cpu->p);
                } else {
                    printf("Unknown flag: %s\n", flag_name);
                }
            } else {
                printf("Usage: flag <N|V|B|D|I|Z|C> <0|1>\n");
            }
        } else if (strcmp(cmd, "run") == 0) {
            while (1) {
                int tr = handle_trap(symbols, cpu, mem, *p_handlers);
                if (tr < 0) break;
                if (tr > 0) continue;
                unsigned char opc = mem_read(mem, cpu->pc);
                if (opc == 0x00) break;  /* BRK */
                const dispatch_entry_t *te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
                if (te->mnemonic && strcmp(te->mnemonic, "STP") == 0) break;
                if (breakpoint_hit(breakpoints, cpu->pc)) break;
                execute_from_mem(cpu, mem, dt, *p_cpu_type);
                if (cpu->cycles > 100000000) break;
            }
            printf("STOP at %04X\n", cpu->pc);
        } else if (strcmp(cmd, "processors") == 0) list_processors();
        else if (strcmp(cmd, "info") == 0) {
            char mnemonic[16];
            if (sscanf(line, "%*s %15s", mnemonic) == 1) print_opcode_info(*p_handlers, *p_num_handlers, mnemonic);
        } else if (strcmp(cmd, "processor") == 0) {
            char type_str[16];
            if (sscanf(line, "%*s %15s", type_str) == 1) {
                if (strcmp(type_str, "6502") == 0) { *p_handlers = opcodes_6502; *p_num_handlers = OPCODES_6502_COUNT; *p_cpu_type = CPU_6502; }
                else if (strcmp(type_str, "6502-undoc") == 0) { *p_handlers = opcodes_6502_undoc; *p_num_handlers = OPCODES_6502_UNDOC_COUNT; *p_cpu_type = CPU_6502_UNDOCUMENTED; }
                else if (strcmp(type_str, "65c02") == 0) { *p_handlers = opcodes_65c02; *p_num_handlers = OPCODES_65C02_COUNT; *p_cpu_type = CPU_65C02; }
                else if (strcmp(type_str, "65ce02") == 0) { *p_handlers = opcodes_65ce02; *p_num_handlers = OPCODES_65CE02_COUNT; *p_cpu_type = CPU_65CE02; }
                else if (strcmp(type_str, "45gs02") == 0) { *p_handlers = opcodes_45gs02; *p_num_handlers = OPCODES_45GS02_COUNT; *p_cpu_type = CPU_45GS02; }
                dispatch_build(dt, *p_handlers, *p_num_handlers, *p_cpu_type);
                printf("Processor set to %s\n", type_str);
            }
        } else if (strcmp(cmd, "regs") == 0) {
            if (*p_cpu_type == CPU_45GS02)
                printf("REGS A=%02X X=%02X Y=%02X Z=%02X B=%02X S=%02X P=%02X PC=%04X Cycles=%lu\n", cpu->a, cpu->x, cpu->y, cpu->z, cpu->b, cpu->s, cpu->p, cpu->pc, cpu->cycles);
            else
                printf("REGS A=%02X X=%02X Y=%02X S=%02X P=%02X PC=%04X Cycles=%lu\n", cpu->a, cpu->x, cpu->y, cpu->s, cpu->p, cpu->pc, cpu->cycles);
        } else if (strcmp(cmd, "mem") == 0) {
            unsigned int addr, len = 16;
            if (sscanf(line + 3, "%x %u", &addr, &len) >= 1) {
                for (unsigned int i = 0; i < len; i++) {
                    if (i % 16 == 0) printf("\n%04X: ", addr + i);
                    printf("%02X ", mem->mem[addr + i]);
                }
                printf("\n");
            }
        } else if (strcmp(cmd, "write") == 0) {
            unsigned int addr, val;
            if (sscanf(line + 5, "%x %x", &addr, &val) == 2) mem_write(mem, addr, (unsigned char)val);
        } else if (strcmp(cmd, "reset") == 0) {
            cpu_init(cpu); cpu->pc = start_addr;
            if (*p_cpu_type == CPU_45GS02) set_flag(cpu, FLAG_E, 1);
        } else if (strcmp(cmd, "step") == 0) {
            int steps = 1;
            if (sscanf(line + 4, "%d", &steps) != 1) steps = 1;
            for (int i = 0; i < steps; i++) {
                int tr = handle_trap(symbols, cpu, mem, *p_handlers);
                if (tr < 0) break;
                if (tr > 0) continue;
                unsigned char opc = mem_read(mem, cpu->pc);
                if (opc == 0x00) break;  /* BRK */
                execute_from_mem(cpu, mem, dt, *p_cpu_type);
            }
            printf("STOP %04X\n", cpu->pc);
        }
    }
}

int main(int argc, char *argv[]) {
	cpu_type_t cpu_type = CPU_6502;
	const char *filename = NULL;
	int interactive_mode = 0;
	breakpoint_list_t breakpoints;
	trace_t trace_info;
	const char *trace_file = NULL;
	int enable_trace = 0;
	unsigned short mem_start = 0, mem_end = 0;
	int show_memory = 0;
	const char *symbol_file = NULL;
	int show_symbols = 0;
	unsigned short start_addr = 0;
	const char *start_label = NULL;
	int start_addr_provided = 0;
	const char *info_mnemonic = NULL;
	
	symbol_table_t symbols;
	breakpoint_init(&breakpoints);
	trace_init(&trace_info);
	symbol_table_init(&symbols, "Default");
	memset(rom, 0, sizeof(rom));
	
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { print_help(argv[0]); return 0; }
		else if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--interactive") == 0) interactive_mode = 1;
		else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) { list_processors(); return 0; }
		else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--opcodes") == 0) {
			if (i + 1 < argc) {
				const char *arg = argv[i+1];
				if (strcmp(arg, "6502") == 0) { list_opcodes(CPU_6502); return 0; }
				else if (strcmp(arg, "6502-undoc") == 0) { list_opcodes(CPU_6502_UNDOCUMENTED); return 0; }
				else if (strcmp(arg, "65c02") == 0) { list_opcodes(CPU_65C02); return 0; }
				else if (strcmp(arg, "65ce02") == 0) { list_opcodes(CPU_65CE02); return 0; }
				else if (strcmp(arg, "45gs02") == 0) { list_opcodes(CPU_45GS02); return 0; }
				else { info_mnemonic = arg; i++; }
			}
		} else if (strcmp(argv[i], "--info") == 0) { if (i + 1 < argc) { info_mnemonic = argv[i+1]; i++; } }
		else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--break") == 0) { if (i + 1 < argc) breakpoint_add(&breakpoints, (unsigned short)strtol(argv[++i], NULL, 16)); }
		else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--address") == 0) {
			if (i + 1 < argc) {
				if (argv[i+1][0] == '$') { start_addr = (unsigned short)strtol(argv[i+1] + 1, NULL, 16); start_addr_provided = 1; }
				else if (isdigit(argv[i+1][0])) { start_addr = (unsigned short)strtol(argv[i+1], NULL, 16); start_addr_provided = 1; }
				else start_label = argv[i+1];
				i++;
			}
		} else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--trace") == 0) {
			enable_trace = 1;
			if (i + 1 < argc && argv[i+1][0] != '-') trace_file = argv[++i];
		} else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mem") == 0) {
			if (i + 1 < argc) {
				char temp[256]; strncpy(temp, argv[++i], 255);
				char *delim = strchr(temp, ':');
				if (delim) { *delim = 0; mem_start = (unsigned short)strtol(temp, NULL, 16); mem_end = (unsigned short)strtol(delim + 1, NULL, 16); show_memory = 1; }
			}
		} else if (strcmp(argv[i], "--symbols") == 0) { if (i + 1 < argc) symbol_file = argv[++i]; }
		else if (strcmp(argv[i], "--preset") == 0) {
			if (i + 1 < argc) {
				const char *p = argv[++i];
				const char *rel = NULL;
				if (strcmp(p, "c64") == 0) rel = "symbols/c64.sym";
				else if (strcmp(p, "c128") == 0) rel = "symbols/c128.sym";
				else if (strcmp(p, "mega65") == 0) rel = "symbols/mega65.sym";
				else if (strcmp(p, "x16") == 0) rel = "symbols/x16.sym";
				if (rel) {
					static char preset_buf[512];
					char exe[512]; exe[0] = 0;
					ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
					if (n > 0) {
						exe[n] = 0;
						char *slash = strrchr(exe, '/');
						if (slash) { *slash = 0; snprintf(preset_buf, sizeof(preset_buf), "%s/%s", exe, rel); rel = preset_buf; }
					}
					symbol_file = rel;
				}
			}
		} else if (strcmp(argv[i], "--show-symbols") == 0) show_symbols = 1;
		else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--processor") == 0) {
			if (i + 1 < argc) {
				const char *p = argv[++i];
				if (strcmp(p, "6502") == 0) cpu_type = CPU_6502;
				else if (strcmp(p, "6502-undoc") == 0) cpu_type = CPU_6502_UNDOCUMENTED;
				else if (strcmp(p, "65c02") == 0) cpu_type = CPU_65C02;
				else if (strcmp(p, "65ce02") == 0) cpu_type = CPU_65CE02;
				else if (strcmp(p, "45gs02") == 0) cpu_type = CPU_45GS02;
			}
		} else if (argv[i][0] != '-' && !filename) filename = argv[i];
	}

	opcode_handler_t *handlers = opcodes_6502;
	int num_handlers = OPCODES_6502_COUNT;

	if (cpu_type == CPU_65C02) { handlers = opcodes_65c02; num_handlers = OPCODES_65C02_COUNT; }
	else if (cpu_type == CPU_65CE02) { handlers = opcodes_65ce02; num_handlers = OPCODES_65CE02_COUNT; }
	else if (cpu_type == CPU_45GS02) { handlers = opcodes_45gs02; num_handlers = OPCODES_45GS02_COUNT; }
	else if (cpu_type == CPU_6502_UNDOCUMENTED) { handlers = opcodes_6502_undoc; num_handlers = OPCODES_6502_UNDOC_COUNT; }

	if (info_mnemonic) { print_opcode_info(handlers, num_handlers, info_mnemonic); return 0; }
	if (!filename) { fprintf(stderr, "Usage: %s [options] <file.asm>\n", argv[0]); return 1; }

	if (symbol_file) symbol_load_file(&symbols, symbol_file);
	FILE *f = fopen(filename, "r");
	if (!f) { perror("fopen"); return 1; }

	char line[128];
	int pc = start_addr_provided ? start_addr : 0x0200;
	while (fgets(line, sizeof(line), f)) {
		if (line[0] == '.') { parse_pseudo_op(line, &cpu_type); continue; }
		char *ptr = line;
		while (*ptr && isspace(*ptr)) ptr++;
		if (!*ptr || *ptr == ';' || *ptr == '.') continue;
		char *colon = strchr(ptr, ':');
		if (colon) {
			char label_name[64]; int len = colon - ptr; if (len >= 64) len = 63;
			strncpy(label_name, ptr, len); label_name[len] = 0;
			symbol_add(&symbols, label_name, pc, SYM_LABEL, "Source");
		}
		instruction_t instr; parse_line(line, &instr, NULL, pc);
		if (instr.op[0]) {
			int len;
			if (strcmp(instr.op, "BRK") == 0) {
				len = 2;
			} else {
				len = get_encoded_length(instr.op, instr.mode, handlers, num_handlers);
				if (len < 0) len = get_instruction_length(instr.mode);
			}
			pc += len;
		}
	}

	if (start_label && !start_addr_provided) {
		if (!symbol_lookup_name(&symbols, start_label, &start_addr)) return 1;
		start_addr_provided = 1;
	}

	/* Finalise handler selection after first pass (may have changed via .processor) */
	if (cpu_type == CPU_65C02) { handlers = opcodes_65c02; num_handlers = OPCODES_65C02_COUNT; }
	else if (cpu_type == CPU_65CE02) { handlers = opcodes_65ce02; num_handlers = OPCODES_65CE02_COUNT; }
	else if (cpu_type == CPU_45GS02) { handlers = opcodes_45gs02; num_handlers = OPCODES_45GS02_COUNT; }
	else if (cpu_type == CPU_6502_UNDOCUMENTED) { handlers = opcodes_6502_undoc; num_handlers = OPCODES_6502_UNDOC_COUNT; }

	cpu_t cpu; cpu_init(&cpu); cpu.pc = start_addr_provided ? start_addr : 0x0200;
	/* 45GS02 resets in emulation mode (E=1): 8-bit SP on page 1, compatible with 6502 */
	if (cpu_type == CPU_45GS02) set_flag(&cpu, FLAG_E, 1);
	memory_t mem; memset(&mem, 0, sizeof(mem));

	/* Second pass: populate rom[] and encode bytes into mem[] */
	rewind(f);
	pc = start_addr_provided ? start_addr : 0x0200;
	while (fgets(line, sizeof(line), f)) {
		if (line[0] == '.' || line[0] == ';') continue;
		instruction_t instr; parse_line(line, &instr, &symbols, pc);
		if (instr.op[0]) {
			rom[pc] = instr;  /* debug overlay */
			int enc = encode_to_mem(&mem, pc, &instr, handlers, num_handlers);
			/* BRK uses 2-byte convention (opcode + pad), even though encode writes 1 */
			if (strcmp(instr.op, "BRK") == 0)
				pc += 2;
			else if (enc > 0)
				pc += enc;
			else
				pc += get_instruction_length(instr.mode);
		}
	}
	fclose(f);

	/* Build byte-indexed dispatch table */
	dispatch_table_t dt;
	dispatch_build(&dt, handlers, num_handlers, cpu_type);

	if (enable_trace && trace_file) trace_enable_file(&trace_info, trace_file);
	else if (enable_trace) trace_enable_stdout(&trace_info);

	if (interactive_mode) { run_interactive_mode(&cpu, &mem, rom, &handlers, &num_handlers, &cpu_type, &dt, start_addr_provided ? start_addr : 0x0200, &breakpoints, &symbols); return 0; }

	printf("\nStarting execution at 0x%04X...\n", cpu.pc);
	while (cpu.cycles < 100000) {
		int tr = handle_trap(&symbols, &cpu, &mem, handlers);
		if (tr < 0) break;	/* bad return address — halt */
		if (tr > 0) continue;
		unsigned char opc = mem_read(&mem, cpu.pc);
		if (opc == 0x00) break;  /* BRK halts */
		const dispatch_entry_t *te = peek_dispatch(&cpu, &mem, &dt, cpu_type);
		if (te->mnemonic && strcmp(te->mnemonic, "STP") == 0) break;
		if (breakpoint_hit(&breakpoints, cpu.pc)) break;
		if (trace_info.enabled) {
			const char *mn = te->mnemonic ? te->mnemonic : "???";
			trace_instruction_full(&trace_info, &cpu, mn, mode_name(te->mode), cpu.cycles);
		}
		execute_from_mem(&cpu, &mem, &dt, cpu_type);
	}
	printf("\nExecution Finished.\n");
	if (cpu_type == CPU_45GS02)
		printf("Registers: A=%02X X=%02X Y=%02X Z=%02X B=%02X S=%02X PC=%04X\n", cpu.a, cpu.x, cpu.y, cpu.z, cpu.b, cpu.s, cpu.pc);
	else
		printf("Registers: A=%02X X=%02X Y=%02X S=%02X PC=%04X\n", cpu.a, cpu.x, cpu.y, cpu.s, cpu.pc);

	if (show_memory) memory_dump(&mem, mem_start, mem_end);
	if (show_symbols) symbol_display(&symbols);
	return 0;
}
