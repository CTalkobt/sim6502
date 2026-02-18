#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "cycles.h"
#include "breakpoint.h"
#include "trace.h"
#include "memory_viewer.h"
#include "interrupt_manager.h"
#include "symbol_table.h"
#include "symbol_table.h"

typedef struct {
	char op[4];
	unsigned char mode;
	unsigned short arg;
} instruction_t;

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
	case MODE_ZP_INDIRECT: return "ZPI";
	case MODE_ABS_INDIRECT_Y: return "AIY";
	default: return "???";
	}
}

static void print_help(const char *progname) {
	printf("6502 Simulator - Complete Debugging & System Tools\n\n");
	printf("Usage: %s [options] <file.asm>\n\n", progname);
	printf("Debugging Options:\n");
	printf("  -a, --address <ADDR>     Start execution at address or label\n");
	printf("  -b, --break <ADDR>       Set breakpoint at address (hex)\n");
	printf("  -t, --trace [FILE]       Enable trace (optional: to file)\n");
	printf("Processor Options:\n");
	printf("  -p, --processor <CPU>    Select processor (6502, 65c02, etc.)\n");
	printf("  -l, --list               List available processors\n");
	printf("  -o, --opcodes <CPU>      List all opcodes for processor\n");
	printf("  -i, --info <OPCODE>      Show detailed information about an opcode\n");
	printf("Memory Options:\n");
	printf("  -m, --mem <RANGE>        View memory (format: 0x1000:0x1100)\n");
	printf("  -s, --stats              Show memory statistics\n");
	printf("Symbol Table Options:\n");
	printf("  --symbols <FILE>         Load custom symbol table\n");
	printf("  --preset <n>             Load preset symbols (c64, c128, mega65, x16)\n");
	printf("  --show-symbols           Display loaded symbols\n");
	printf("Interrupt Options:\n");
	printf("  --irq <CYCLE>            Trigger IRQ at cycle count\n");
	printf("  -n, --nmi <CYCLE>        Trigger NMI at cycle count\n");
	printf("Other:\n");
	printf("  -h, --help               Show this help\n\n");
	printf("Examples:\n");
	printf("  %s program.asm\n", progname);
	printf("  %s -b 0x1000 -t trace.log program.asm\n", progname);
	printf("  %s --preset c64 program.asm\n", progname);
	printf("  %s --symbols custom.sym program.asm\n\n", progname);
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

static void print_opcode_info(const char *mnemonic) {
	char upper_mnemonic[16];
	int i;
	for (i = 0; i < 15 && mnemonic[i]; i++)
		upper_mnemonic[i] = toupper(mnemonic[i]);
	upper_mnemonic[i] = 0;

	printf("Instruction Reference: %s\n", upper_mnemonic);
	printf("--------------------------------------------------------------------------------\n");
	printf("%-10s %-10s %-8s %-8s %-8s %-8s %-8s\n", "Mode", "Mnemonic", "6502", "Undoc", "65C02", "65CE02", "45GS02");
	printf("--------------------------------------------------------------------------------\n");

	struct {
		const char *name;
		opcode_handler_t *handlers;
		int count;
	} variants[] = {
		{"6502", opcodes_6502, OPCODES_6502_COUNT},
		{"6502-undoc", opcodes_6502_undoc, OPCODES_6502_UNDOC_COUNT},
		{"65C02", opcodes_65c02, OPCODES_65C02_COUNT},
		{"65CE02", opcodes_65ce02, OPCODES_65CE02_COUNT},
		{"45GS02", opcodes_45gs02, OPCODES_45GS02_COUNT}
	};

	/* We want to list all unique mode/mnemonic combinations for this instruction */
	for (int mode = 0; mode <= MODE_ABS_INDIRECT_Y; mode++) {
		int found_any = 0;
		int presence[5] = {0, 0, 0, 0, 0};
		unsigned char cycles[5] = {0, 0, 0, 0, 0};
		
		for (int v = 0; v < 5; v++) {
			for (int h = 0; h < variants[v].count; h++) {
				if (strcmp(variants[v].handlers[h].mnemonic, upper_mnemonic) == 0 &&
				    variants[v].handlers[h].mode == mode) {
					found_any = 1;
					presence[v] = 1;
					cycles[v] = variants[v].handlers[h].cycles_6502;
					break;
				}
			}
		}

		if (found_any) {
			printf("%-10s %-10s", mode_name(mode), upper_mnemonic);
			for (int v = 0; v < 5; v++) {
				if (presence[v]) {
					if (cycles[v] > 0) {
						printf(" %-8d", cycles[v]);
					} else {
						printf(" %-8s", "V");
					}
				} else {
					printf(" %-8s", "");
				}
			}
			printf("\n");
		}
	}
	printf("\n");
}

static void scan_labels(const char *filename, symbol_table_t *symbols) {
	FILE *f = fopen(filename, "r");
	if (!f) return;
	
	char line[128];
	int pc_addr = 0;
	while (fgets(line, sizeof(line), f)) {
		char *ptr = line;
		while (*ptr && isspace(*ptr)) ptr++;
		
		if (!*ptr || *ptr == ';' || *ptr == '.') continue;
		
		char *colon = strchr(ptr, ':');
		if (colon) {
			char label_name[64];
			int len = colon - ptr;
			if (len >= 64) len = 63;
			strncpy(label_name, ptr, len);
			label_name[len] = 0;
			symbol_add(symbols, label_name, pc_addr, SYM_LABEL, "From source");
			
			ptr = colon + 1;
			while (*ptr && isspace(*ptr)) ptr++;
		}
		
		if (*ptr && *ptr != ';') {
			pc_addr++;
		}
	}
	fclose(f);
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

static void parse_line(const char *line, instruction_t *instr) {
	int i = 0;
	
	while (*line && isspace(*line)) line++;
	if (!*line || *line == ';' || *line == '.') {
		instr->op[0] = 0;
		return;
	}
	
	/* Skip label if present */
	const char *colon = strchr(line, ':');
	if (colon) {
		line = colon + 1;
		while (*line && isspace(*line)) line++;
	}

	if (!*line || *line == ';' || *line == '.') {
		instr->op[0] = 0;
		return;
	}
	
	while (*line && !isspace(*line) && i < 3)
		instr->op[i++] = toupper(*line++);
	instr->op[i] = 0;
	
	while (*line && isspace(*line)) line++;
	
	instr->mode = MODE_IMPLIED;
	instr->arg = 0;
	
	if (*line == '#') {
		instr->mode = MODE_IMMEDIATE;
		line++;
		if (*line == '$') line++;
		instr->arg = (unsigned short)strtol(line, NULL, 16);
	} else if (*line == '$') {
		line++;
		instr->arg = (unsigned short)strtol(line, NULL, 16);
		
		while (*line && !isspace(*line) && *line != ',') line++;
		while (*line && (isspace(*line) || *line == ',')) line++;
		
		if (toupper(*line) == 'X') {
			instr->mode = MODE_ABSOLUTE_X;
		} else if (toupper(*line) == 'Y') {
			instr->mode = MODE_ABSOLUTE_Y;
		} else {
			instr->mode = MODE_ABSOLUTE;
		}
	} else if (*line == '(') {
		line++;
		if (*line == '$') {
			line++;
			instr->arg = (unsigned short)strtol(line, NULL, 16);
			while (*line && *line != ')') line++;
			if (*line == ')') line++;
			while (*line && isspace(*line)) line++;
			
			if (*line == ',') {
				line++;
				while (*line && isspace(*line)) line++;
				if (toupper(*line) == 'X') {
					instr->mode = MODE_INDIRECT_X;
				}
			} else if (toupper(*line) == 'Y') {
				instr->mode = MODE_INDIRECT_Y;
			} else {
				instr->mode = MODE_INDIRECT;
			}
		}
	} else if (instr->op[0]) {
		instr->mode = MODE_IMPLIED;
	}
}

static void execute(cpu_t *cpu, memory_t *mem, instruction_t *instr, 
		const opcode_handler_t *handlers, int num_handlers) {
	if (!instr->op[0]) return;
	
	for (int i = 0; i < num_handlers; i++) {
		if (strcmp(instr->op, handlers[i].mnemonic) == 0 && 
		    handlers[i].mode == instr->mode) {
			handlers[i].fn(cpu, mem, instr->arg);
			return;
		}
	}
}

int main(int argc, char *argv[]) {
	cpu_type_t cpu_type = CPU_6502;
	const char *filename = NULL;
	breakpoint_list_t breakpoints;
	trace_t trace_info;
	const char *trace_file = NULL;
	int enable_trace = 0;
	
	memory_viewer_t mem_viewer;
	unsigned short mem_start = 0, mem_end = 0;
	int show_memory = 0;
	int show_stats = 0;
	
	interrupt_manager_t irq_mgr;
	unsigned long irq_trigger = 0, nmi_trigger = 0;
	int irq_enabled = 0, nmi_enabled = 0;
	
	symbol_table_t symbols;
	const char *symbol_file = NULL;
	int show_symbols = 0;
	
	unsigned short start_addr = 0;
	const char *start_label = NULL;
	int start_addr_provided = 0;
	
	breakpoint_init(&breakpoints);
	trace_init(&trace_info);
	memory_viewer_init(&mem_viewer);
	interrupt_init(&irq_mgr);
	symbol_table_init(&symbols, "Default");
	
	/* Parse command line */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			print_help(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
			list_processors();
			return 0;
		} else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--opcodes") == 0) {
			if (i + 1 < argc) {
				cpu_type_t list_type = CPU_6502;
				if (strcmp(argv[i+1], "6502") == 0) list_type = CPU_6502;
				else if (strcmp(argv[i+1], "6502-undoc") == 0) list_type = CPU_6502_UNDOCUMENTED;
				else if (strcmp(argv[i+1], "65c02") == 0) list_type = CPU_65C02;
				else if (strcmp(argv[i+1], "65ce02") == 0) list_type = CPU_65CE02;
				else if (strcmp(argv[i+1], "45gs02") == 0) list_type = CPU_45GS02;
				list_opcodes(list_type);
				return 0;
			}
		} else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--info") == 0) {
			if (i + 1 < argc) {
				print_opcode_info(argv[i+1]);
				return 0;
			}
		} else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--break") == 0) {
			if (i + 1 < argc) {
				unsigned short addr = (unsigned short)strtol(argv[i+1], NULL, 16);
				breakpoint_add(&breakpoints, addr);
				printf("Breakpoint set at 0x%04X\n", addr);
				i++;
			}
		} else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--address") == 0) {
			if (i + 1 < argc) {
				if (argv[i+1][0] == '$') {
					start_addr = (unsigned short)strtol(argv[i+1] + 1, NULL, 16);
					start_addr_provided = 1;
				} else if (isdigit(argv[i+1][0])) {
					start_addr = (unsigned short)strtol(argv[i+1], NULL, 16);
					start_addr_provided = 1;
				} else {
					start_label = argv[i+1];
				}
				i++;
			}
		} else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--trace") == 0) {
			enable_trace = 1;
			if (i + 1 < argc && argv[i+1][0] != '-') {
				trace_file = argv[i+1];
				i++;
			}
		} else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mem") == 0) {
			if (i + 1 < argc) {
				char temp[256];
				strncpy(temp, argv[i+1], sizeof(temp) - 1);
				char *delim = strchr(temp, ':');
				if (!delim) delim = strchr(temp, '-');
				if (delim) {
					*delim = 0;
					mem_start = (unsigned short)strtol(temp, NULL, 16);
					mem_end = (unsigned short)strtol(delim + 1, NULL, 16);
					show_memory = 1;
				}
				i++;
			}
		} else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
			show_stats = 1;
		} else if (strcmp(argv[i], "--irq") == 0) {
			if (i + 1 < argc) {
				irq_trigger = (unsigned long)strtol(argv[i+1], NULL, 10);
				irq_enabled = 1;
				printf("IRQ scheduled at cycle %lu\n", irq_trigger);
				i++;
			}
		} else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--nmi") == 0) {
			if (i + 1 < argc) {
				nmi_trigger = (unsigned long)strtol(argv[i+1], NULL, 10);
				nmi_enabled = 1;
				printf("NMI scheduled at cycle %lu\n", nmi_trigger);
				i++;
			}
		} else if (strcmp(argv[i], "--symbols") == 0) {
			if (i + 1 < argc) {
				symbol_file = argv[i+1];
				i++;
			}
		} else if (strcmp(argv[i], "--preset") == 0) {
			if (i + 1 < argc) {
				const char *preset = argv[i+1];
				
				if (strcmp(preset, "c64") == 0) {
					symbol_file = "symbols/c64.sym";
				} else if (strcmp(preset, "c128") == 0) {
					symbol_file = "symbols/c128.sym";
				} else if (strcmp(preset, "mega65") == 0) {
					symbol_file = "symbols/mega65.sym";
				} else if (strcmp(preset, "x16") == 0) {
					symbol_file = "symbols/x16.sym";
				} else {
					fprintf(stderr, "Unknown preset: %s\n", preset);
					fprintf(stderr, "Valid presets: c64, c128, mega65, x16\n");
					i++;
					continue;
				}
				i++;
			}
		} else if (strcmp(argv[i], "--show-symbols") == 0) {
			show_symbols = 1;
		} else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--processor") == 0) {
			if (i + 1 < argc) {
				if (strcmp(argv[i+1], "6502") == 0) cpu_type = CPU_6502;
				else if (strcmp(argv[i+1], "6502-undoc") == 0) cpu_type = CPU_6502_UNDOCUMENTED;
				else if (strcmp(argv[i+1], "65c02") == 0) cpu_type = CPU_65C02;
				else if (strcmp(argv[i+1], "65ce02") == 0) cpu_type = CPU_65CE02;
				else if (strcmp(argv[i+1], "45gs02") == 0) cpu_type = CPU_45GS02;
				i++;
			}
		} else if (argv[i][0] != '-' && !filename) {
			filename = argv[i];
		}
	}
	
	if (!filename) {
		fprintf(stderr, "Usage: %s [options] <file.asm>\n", argv[0]);
		return 1;
	}
	
	/* Load symbol table if specified */
	if (symbol_file) {
		int count = symbol_load_file(&symbols, symbol_file);
		if (count > 0) {
			printf("Loaded %d symbols from %s\n", count, symbol_file);
			strncpy(symbols.arch_name, symbol_file, sizeof(symbols.arch_name) - 1);
		} else {
			fprintf(stderr, "Warning: No symbols loaded from %s\n", symbol_file);
		}
	}
	
	if (enable_trace) {
		if (trace_file) {
			if (!trace_enable_file(&trace_info, trace_file)) {
				fprintf(stderr, "Error: Could not open trace file\n");
				return 1;
			}
		} else {
			trace_enable_stdout(&trace_info);
		}
	}
	
	/* Scan for labels in the assembly file */
	scan_labels(filename, &symbols);
	
	if (start_label) {
		if (!symbol_lookup_name(&symbols, start_label, &start_addr)) {
			fprintf(stderr, "Error: Label '%s' not found\n", start_label);
			return 1;
		}
		start_addr_provided = 1;
	}
	
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror("fopen");
		return 1;
	}
	
	cpu_t cpu;
	cpu_init(&cpu);
	
	if (start_addr_provided) {
		cpu.pc = start_addr;
	}
	
	memory_t mem = {{0}, 0};
	
	char line[128];
	int max_cycles = 100000;
	int instr_count = 0;
	
	opcode_handler_t *handlers = NULL;
	int num_handlers = 0;
	
	if (cpu_type == CPU_6502_UNDOCUMENTED) {
		handlers = opcodes_6502_undoc;
		num_handlers = OPCODES_6502_UNDOC_COUNT;
	} else if (cpu_type == CPU_65C02) {
		handlers = opcodes_65c02;
		num_handlers = OPCODES_65C02_COUNT;
	} else if (cpu_type == CPU_65CE02) {
		handlers = opcodes_65ce02;
		num_handlers = OPCODES_65CE02_COUNT;
	} else if (cpu_type == CPU_45GS02) {
		handlers = opcodes_45gs02;
		num_handlers = OPCODES_45GS02_COUNT;
	} else {
		handlers = opcodes_6502;
		num_handlers = OPCODES_6502_COUNT;
	}
	
	int pc_addr = 0;
	while (fgets(line, sizeof(line), f) && instr_count < max_cycles) {
		if (line[0] == '.') {
			parse_pseudo_op(line, &cpu_type);
			continue;
		}
		
		instruction_t instr;
		parse_line(line, &instr);
		if (!instr.op[0]) continue;
		
		if (pc_addr == cpu.pc) {
			/* Check for scheduled interrupts */
			if (irq_enabled && cpu.cycles >= irq_trigger) {
				interrupt_request_irq(&irq_mgr);
				irq_enabled = 0;
			}
			
			if (nmi_enabled && cpu.cycles >= nmi_trigger) {
				interrupt_request_nmi(&irq_mgr);
				nmi_enabled = 0;
			}
			
			/* Check if interrupt should be taken */
			if (interrupt_check(&irq_mgr, &cpu)) {
				printf("\n>>> %s at cycle %lu\n", 
					interrupt_type_name(irq_mgr.current.type), cpu.cycles);
				interrupt_handle(&irq_mgr, &cpu, &mem);
				continue;
			}
			
			/* Check breakpoint */
			if (breakpoint_hit(&breakpoints, cpu.pc)) {
				printf("\n*** BREAKPOINT at 0x%04X ***\n", cpu.pc);
				printf("Registers: A=%02X X=%02X Y=%02X S=%02X P=%02X\n",
					cpu.a, cpu.x, cpu.y, cpu.s, cpu.p);
				break;
			}
			
			unsigned long prev_cycles = cpu.cycles;
			int prev_pc = cpu.pc;
			
			if (trace_info.enabled) {
				trace_instruction_full(&trace_info, &cpu, instr.op, 
					mode_name(instr.mode), prev_cycles);
			}
			
			execute(&cpu, &mem, &instr, handlers, num_handlers);
			if (cpu.pc == prev_pc) cpu.pc += 1;
			
			instr_count++;
		}
		pc_addr++;
	}
	fclose(f);
	
	trace_disable(&trace_info);
	
	printf("\n6502 Simulator - %s\n", processor_name(cpu_type));
	printf("═══════════════════════════════════════════════════\n");
	printf("Registers:\n");
	printf("  A: 0x%02X     X: 0x%02X     Y: 0x%02X     S: 0x%02X\n", 
		cpu.a, cpu.x, cpu.y, cpu.s);
	if (cpu_type == CPU_45GS02) {
		printf("  Z: 0x%02X\n", cpu.z);
	}
	printf("  PC: 0x%04X   P: 0x%02X\n", cpu.pc, cpu.p);
	printf("\nProcessor Flags: N=%d V=%d B=%d D=%d I=%d Z=%d C=%d\n",
		(cpu.p >> 7) & 1, (cpu.p >> 6) & 1, (cpu.p >> 4) & 1, (cpu.p >> 3) & 1,
		(cpu.p >> 2) & 1, (cpu.p >> 1) & 1, (cpu.p >> 0) & 1);
	
	printf("\nExecution Statistics:\n");
	printf("  Instructions: %d\n", instr_count);
	printf("  Cycles: %lu\n", cpu.cycles);
	printf("  Avg: %.1f cycles/instr\n", cpu.cycles > 0 ? (double)cpu.cycles / instr_count : 0.0);
	
	if (mem.mem_writes > 0) {
		printf("\nMemory Writes: %d\n", mem.mem_writes);
		for (int i = 0; i < mem.mem_writes && i < 5; i++)
			printf("  [0x%04X] = 0x%02X\n", mem.mem_addr[i], mem.mem_val[i]);
		if (mem.mem_writes > 5)
			printf("  ... and %d more\n", mem.mem_writes - 5);
	}
	
	if (show_memory) {
		memory_dump(&mem, mem_start, mem_end);
	}
	
	if (show_stats) {
		memory_stats(&mem);
	}
	
	if (irq_mgr.interrupt_count > 0) {
		interrupt_show_status(&irq_mgr);
	}
	
	if (show_symbols && symbols.count > 0) {
		symbol_display(&symbols);
	}
	
	return 0;
}
