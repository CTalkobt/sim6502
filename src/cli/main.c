#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "cycles.h"
#include "breakpoints.h"
#include "trace.h"
#include "memory_utils.h"
#include "interrupts.h"
#include "symbols.h"
#include "assembler.h"
#include "disassembler.h"
#include "commands.h"
#include "cpu_engine.h"

static instruction_t rom[65536];

static int handle_trap_main(const symbol_table_t *st, cpu_t *cpu, memory_t *mem) {
    for (int i = 0; i < st->count; i++) {
        if (st->symbols[i].type != SYM_TRAP) continue;
        if (st->symbols[i].address != cpu->pc) continue;
        
        printf("[TRAP] %-20s $%04X  A=%02X X=%02X Y=%02X",
            st->symbols[i].name, cpu->pc, cpu->a, cpu->x, cpu->y);
        if (cpu->pc > 0) printf(" Z=%02X B=%02X", cpu->z, cpu->b);
        printf(" S=%02X P=%02X", cpu->s, cpu->p);
        if (st->symbols[i].comment[0]) printf("  ; %s", st->symbols[i].comment);
        printf("\n");

        cpu->cycles += 6;
        cpu->s++;
        unsigned short lo = mem_read(mem, 0x100 + cpu->s);
        cpu->s++;
        unsigned short hi = mem_read(mem, 0x100 + cpu->s);
        unsigned short ret = (unsigned short)(((unsigned short)hi << 8) | lo);
        ret++;
        if (ret == 0) return -1;
        cpu->pc = ret;
        return 1;
    }
    return 0;
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
	
	symbol_table_t symbols;
	breakpoint_init(&breakpoints);
	trace_init(&trace_info);
	symbol_table_init(&symbols, "Default");
	memset(rom, 0, sizeof(rom));
	
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { print_help(argv[0]); return 0; }
		else if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--interactive") == 0) interactive_mode = 1;
		else if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0) cli_set_json_mode(1);
		else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) { list_processors(); return 0; }
		else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--break") == 0) { 
			if (i + 1 < argc) {
				const char *p = argv[++i];
				unsigned long addr;
				if (parse_mon_value(&p, &addr)) {
					while (*p && isspace((unsigned char)*p)) p++;
					breakpoint_add(&breakpoints, (unsigned short)addr, *p ? p : NULL);
				}
			}
		}
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
				if (memview_parse_range(argv[++i], &mem_start, &mem_end)) show_memory = 1;
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
		}
		else if (strcmp(argv[i], "--show-symbols") == 0) show_symbols = 1;
		else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--processor") == 0) {
			if (i + 1 < argc) {
				const char *p = argv[++i];
				if (strcmp(p, "6502") == 0) cpu_type = CPU_6502;
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

	if (!filename && !interactive_mode) { print_help(argv[0]); return 1; }
	if (symbol_file) symbol_load_file(&symbols, symbol_file);

	cpu_t cpu;
	memory_t mem; memset(&mem, 0, sizeof(mem));

	if (filename) {
		FILE *f = fopen(filename, "r");
		if (!f) { perror("fopen"); return 1; }
		int pc = start_addr_provided ? start_addr : 0x0200;
		char line[512];
		while (fgets(line, sizeof(line), f)) {
			const char *ptr = line; while (*ptr && isspace(*ptr)) ptr++;
			if (!*ptr || *ptr == ';') continue;
			const char *semi1 = strchr(ptr, ';');
			const char *colon = strchr(ptr, ':');
			/* Dot-prefixed local label (.name:) must be checked before pseudo-op */
			if (*ptr == '.' && !(colon && (!semi1 || colon < semi1))) {
				handle_pseudo_op(ptr, &cpu_type, &pc, NULL, NULL, NULL); continue;
			}
			if (colon && (!semi1 || colon < semi1)) {
				char l[64]; int len = colon - ptr; if (len >= 64) len = 63;
				strncpy(l, ptr, len); l[len] = 0; symbol_add(&symbols, l, pc, SYM_LABEL, "Source");
				const char *after = colon + 1; while (*after && isspace(*after)) after++;
				if (*after == '.') { handle_pseudo_op(after, &cpu_type, &pc, NULL, NULL, NULL); continue; }
			} else {
				/* Equate:  NAME = VALUE  (no colon, '=' before any ';') */
				const char *eq = strchr(ptr, '=');
				if (eq && (!semi1 || eq < semi1)) {
					const char *nend = eq - 1; while (nend > ptr && isspace(*nend)) nend--;
					int nlen = (int)(nend - ptr) + 1;
					if (nlen > 0 && nlen < 64 && (isalpha(*ptr) || *ptr == '_')) {
						char ename[64]; strncpy(ename, ptr, (size_t)nlen); ename[nlen] = 0;
						const char *vp = eq + 1; while (*vp && isspace(*vp)) vp++;
						unsigned long val = (unsigned long)parse_value(vp, NULL);
						symbol_add(&symbols, ename, (int)val, SYM_LABEL, "Equate");
						continue;
					}
				}
			}
			instruction_t instr; parse_line(line, &instr, NULL, pc);
			if (instr.op[0]) {
				pc += get_encoded_length(instr.op, instr.mode, handlers, num_handlers, cpu_type);
			}
		}
		if (start_label && !start_addr_provided) { symbol_lookup_name(&symbols, start_label, &start_addr); start_addr_provided = 1; }
		if (cpu_type == CPU_65C02) { handlers = opcodes_65c02; num_handlers = OPCODES_65C02_COUNT; }
		else if (cpu_type == CPU_65CE02) { handlers = opcodes_65ce02; num_handlers = OPCODES_65CE02_COUNT; }
		else if (cpu_type == CPU_45GS02) { handlers = opcodes_45gs02; num_handlers = OPCODES_45GS02_COUNT; }
		cpu_init(&cpu); cpu.pc = start_addr_provided ? start_addr : 0x0200;
		if (cpu_type == CPU_45GS02) set_flag(&cpu, FLAG_E, 1);
		rewind(f); pc = start_addr_provided ? start_addr : 0x0200;
		while (fgets(line, sizeof(line), f)) {
			const char *ptr = line; while (*ptr && isspace(*ptr)) ptr++;
			if (!*ptr || *ptr == ';') continue;
			const char *semi2 = strchr(ptr, ';');
			const char *colon2 = strchr(ptr, ':');
			/* Dot-prefixed local label (.name:) must be checked before pseudo-op */
			if (*ptr == '.' && !(colon2 && (!semi2 || colon2 < semi2))) {
				handle_pseudo_op(ptr, &cpu_type, &pc, &mem, &symbols, NULL); continue;
			}
			if (colon2 && (!semi2 || colon2 < semi2)) { const char *after = colon2 + 1; while (*after && isspace(*after)) after++; if (*after == '.') { handle_pseudo_op(after, &cpu_type, &pc, &mem, &symbols, NULL); continue; } }
			else {
				/* Skip equate lines in second pass (already in symbol table) */
				const char *eq2 = strchr(ptr, '=');
				if (eq2 && (!semi2 || eq2 < semi2) && (isalpha(*ptr) || *ptr == '_')) continue;
			}
			instruction_t instr; parse_line(line, &instr, &symbols, pc);
			if (instr.op[0]) {
				rom[pc] = instr;
				pc += encode_to_mem(&mem, pc, &instr, handlers, num_handlers, cpu_type);
			}
		}
		fclose(f);
	} else {
		cpu_init(&cpu); cpu.pc = start_addr_provided ? start_addr : 0x0200;
		if (cpu_type == CPU_45GS02) set_flag(&cpu, FLAG_E, 1);
	}

	dispatch_table_t dt; dispatch_build(&dt, handlers, num_handlers, cpu_type);
	if (enable_trace && trace_file) trace_enable_file(&trace_info, trace_file);
	else if (enable_trace) trace_enable_stdout(&trace_info);

	if (interactive_mode) { run_interactive_mode(&cpu, &mem, &handlers, &num_handlers, &cpu_type, &dt, cpu.pc, &breakpoints, &symbols); return 0; }

    printf("\nStarting execution at 0x%04X...\n", cpu.pc);
	while (cpu.cycles < 1000000) {
		int tr = handle_trap_main(&symbols, &cpu, &mem); if (tr < 0) break; if (tr > 0) continue;
		unsigned char opc = mem_read(&mem, cpu.pc);
		if (opc == 0x00) break;
		const dispatch_entry_t *te = peek_dispatch(&cpu, &mem, &dt, cpu_type);
		if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) break;
		if (breakpoint_hit(&breakpoints, &cpu)) { printf("STOP at $%04X\n", cpu.pc); break; }
		if (trace_info.enabled) { trace_instruction_full(&trace_info, &cpu, te->mnemonic, mode_name(te->mode), cpu.cycles); }
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
