#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <vector>
#include <string>
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "cycles.h"
#include "breakpoints.h"
#include "trace.h"
#include "memory_utils.h"
#include "interrupts.h"
#include "symbols.h"
#include "list_parser.h"
#include "metadata.h"
#include "disassembler.h"
#include "commands.h"
#include "cpu_engine.h"
#include "cpu_6502.h"
#include "audio.h"
#include "device/mega65_io.h"
#include "device/vic2_io.h"
#include "device/sid_io.h"
#include "device/cia_io.h"

static std::vector<IOHandler*> g_main_dynamic_handlers;

static int handle_trap_main(const symbol_table_t *st, cpu_t *cpu, memory_t *mem) {
    for (int i = 0; i < st->count; i++) {
        if (st->symbols[i].address != cpu->pc) continue;

        if (st->symbols[i].type == SYM_INSPECT) {
            printf("[INSPECT] %s at $%04X\n", st->symbols[i].name, cpu->pc);
            
            if (strcasecmp(st->symbols[i].name, "cpu") == 0) {
                if (cpu->pc > 0) { // Check if it's a 45GS02-style state (loosely)
                    printf("  REGS: A=%02X X=%02X Y=%02X Z=%02X B=%02X S=%04X P=%02X PC=%04X\n",
                           cpu->a, cpu->x, cpu->y, cpu->z, cpu->b, cpu->s, cpu->p, cpu->pc);
                } else {
                    printf("  REGS: A=%02X X=%02X Y=%02X S=%02X P=%02X PC=%04X\n",
                           cpu->a, cpu->x, cpu->y, (uint8_t)cpu->s, cpu->p, cpu->pc);
                }
            } else {
                /* Try to find device by name */
                IOHandler *h = mem->io_registry ? mem->io_registry->find_handler(st->symbols[i].name) : nullptr;
                if (h) {
                    printf("  Device: %s\n", h->get_handler_name());
                    /* Print registers if they fit in 32 bytes (common for our devices) */
                    printf("  Registers: ");
                    for (int r = 0; r < 32; r++) {
                        uint8_t val = 0;
                        if (h->io_read(mem, (uint16_t)r, &val)) {
                            printf("%02X ", val);
                            if ((r+1)%8 == 0 && r < 31) printf("\n             ");
                        }
                    }
                    printf("\n");
                } else {
                    /* Not a named device, treat name as a potential hex address or just dump around PC */
                    const char *nptr = st->symbols[i].name;
                    if (*nptr == '$') nptr++;
                    unsigned long addr = strtoul(nptr, NULL, 16);
                    if (addr == 0 && nptr[0] != '0') addr = cpu->pc;
                    printf("  Memory at $%04lX: ", addr);
                    for (int r = 0; r < 16; r++) printf("%02X ", mem_read(mem, (uint16_t)(addr + r)));
                    printf("\n");
                }
            }
            continue; /* Check for more symbols at this address */
        }

        if (st->symbols[i].type != SYM_TRAP) continue;
        
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
	machine_type_t machine_type = MACHINE_C64;
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
	std::vector<std::string> initial_cmds;
	int enable_debug = 0;
	unsigned long cycle_limit = 1000000;
	float speed_scale = 1.0f; // Default to C64 speed
	
	symbol_table_t symbols;
	source_map_t source_map;
	breakpoint_init(&breakpoints);
	trace_init(&trace_info);
	symbol_table_init(&symbols, "Default");
	source_map_init(&source_map);
	
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { print_help(argv[0]); return 0; }
		else if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--interactive") == 0) interactive_mode = 1;
		else if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0) cli_set_json_mode(1);
		else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) { list_processors(); return 0; }
		else if (strcmp(argv[i], "-M") == 0 || strcmp(argv[i], "--machine") == 0) {
			if (i + 1 < argc) {
				const char *m = argv[++i];
				if (strcmp(m, "raw6502") == 0) { machine_type = MACHINE_RAW6502; cpu_type = CPU_6502; }
				else if (strcmp(m, "c64") == 0) { machine_type = MACHINE_C64; cpu_type = CPU_6502; symbol_file = "symbols/c64.sym"; }
				else if (strcmp(m, "c128") == 0) { machine_type = MACHINE_C128; cpu_type = CPU_6502; symbol_file = "symbols/c128.sym"; }
				else if (strcmp(m, "mega65") == 0) { machine_type = MACHINE_MEGA65; cpu_type = CPU_45GS02; symbol_file = "symbols/mega65.sym"; }
				else if (strcmp(m, "x16") == 0) { machine_type = MACHINE_X16; cpu_type = CPU_65C02; symbol_file = "symbols/x16.sym"; }
			}
		}
		else if (strcmp(argv[i], "--debug") == 0) {
			enable_debug = 1;
		}
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
		} else if (strcmp(argv[i], "--debug") == 0) {
			enable_debug = 1;
		} else if (strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--limit") == 0) {
			if (i + 1 < argc) cycle_limit = strtoul(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--speed") == 0) {
			if (i + 1 < argc) speed_scale = (float)atof(argv[++i]);
		} else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--command") == 0) {
			if (i + 1 < argc) initial_cmds.push_back(argv[++i]);
		} else if (argv[i][0] != '-' && !filename) filename = argv[i];
	}

	opcode_handler_t *handlers = opcodes_6502;
	int num_handlers = OPCODES_6502_COUNT;
	if (cpu_type == CPU_65C02) { handlers = opcodes_65c02; num_handlers = OPCODES_65C02_COUNT; }
	else if (cpu_type == CPU_65CE02) { handlers = opcodes_65ce02; num_handlers = OPCODES_65CE02_COUNT; }
	else if (cpu_type == CPU_45GS02) { handlers = opcodes_45gs02; num_handlers = OPCODES_45GS02_COUNT; }

	if (!filename && !interactive_mode && initial_cmds.empty()) { print_help(argv[0]); return 1; }
	if (symbol_file) symbol_load_file(&symbols, symbol_file);

	memory_t mem; memset(&mem, 0, sizeof(mem));
	CPU *cpu_ptr = CPUFactory::create(cpu_type);
	cpu_ptr->mem = &mem;
	cpu_ptr->debug = enable_debug != 0;
	mem.io_registry = new IORegistry(cpu_ptr->get_interrupt_controller());
	
    if (filename) {
		/* Toolchain-based loading (replaces legacy internal assembler) */
		char base[512];
		strncpy(base, filename, sizeof(base)-1);
		base[sizeof(base)-1] = 0;
		char *dot = strrchr(base, '.');
		if (dot && (strcasecmp(dot, ".asm") == 0 || strcasecmp(dot, ".s") == 0)) *dot = 0;

		if (!load_toolchain_bundle(&mem, &symbols, &source_map, base)) {
            /* If bundle failed, try loading as raw binary/prg if it's not .asm */
            if (dot && (strcasecmp(dot, ".asm") == 0 || strcasecmp(dot, ".s") == 0)) {
			    fprintf(stderr, "Error: Could not load toolchain bundle for '%s'\n", filename);
			    return 1;
            } else {
                /* Try loading as PRG */
                int prg_addr = 0;
                if (load_prg(&mem, filename, &prg_addr) < 0) {
                    /* Try loading as binary */
                    int b_addr = start_addr_provided ? start_addr : 0x0200;
                    if (load_binary(&mem, b_addr, filename) < 0) {
                        fprintf(stderr, "Error: Could not load '%s'\n", filename);
                        return 1;
                    }
                } else if (!start_addr_provided) {
                    start_addr = (unsigned short)prg_addr;
                    start_addr_provided = 1;
                }
            }
		}

		/* Find start address if not provided */
        if (!start_addr_provided) {
		    unsigned short found_addr = 0x0801;
		    if (symbol_lookup_name(&symbols, "main", &found_addr)) {
                start_addr = found_addr;
                start_addr_provided = 1;
            } else if (symbol_lookup_name(&symbols, "start", &found_addr)) {
                start_addr = found_addr;
                start_addr_provided = 1;
            }
        }
    }

    /* Initialize hardware based on machine type */
    switch (machine_type) {
        case MACHINE_RAW6502: break;
        case MACHINE_MEGA65:  
            mega65_io_register(&mem); 
            sid_io_register(&mem, machine_type, g_main_dynamic_handlers);
            cia_io_register(&mem, g_main_dynamic_handlers);
            break;
        case MACHINE_C64:
        case MACHINE_C128:
        case MACHINE_X16:
        default:              
            vic2_io_register(&mem); 
            sid_io_register(&mem, machine_type, g_main_dynamic_handlers);
            cia_io_register(&mem, g_main_dynamic_handlers);
            mem.io_registry->rebuild_map(&mem); 
            break;
    }

    cpu_ptr->reset(); 
    cpu_ptr->pc = start_addr_provided ? start_addr : (filename ? 0x0801 : 0x0200);
    if (cpu_type == CPU_45GS02) set_flag(cpu_ptr, FLAG_E, 1);

	dispatch_table_t dt; dispatch_build(&dt, handlers, num_handlers, cpu_type);
	if (enable_trace && trace_file) trace_enable_file(&trace_info, trace_file);
	else if (enable_trace) trace_enable_stdout(&trace_info);

	if (interactive_mode || !initial_cmds.empty()) { 
        run_interactive_mode(cpu_ptr, &mem, &handlers, &num_handlers, &cpu_type, &dt, cpu_ptr->pc, &breakpoints, &symbols, initial_cmds); 
        return 0; 
    }
	
    int stop_reason = 0; // 0=limit, 1=brk, 2=stp, 3=bp, 4=trap
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    unsigned long cyc0 = cpu_ptr->cycles;
    const double C64_HZ = 985248.0;

    printf("\nStarting execution at 0x%04X...\n", cpu_ptr->pc);
	while (cpu_ptr->cycles < cycle_limit) {
		int tr = handle_trap_main(&symbols, cpu_ptr, cpu_ptr->mem); 
        if (tr < 0) { stop_reason = 4; break; } 
        if (tr > 0) continue;

		unsigned char opc = mem_read(cpu_ptr->mem, cpu_ptr->pc);
		if (opc == 0x00) { stop_reason = 1; break; }

		const dispatch_entry_t *te = peek_dispatch(cpu_ptr, cpu_ptr->mem, &dt, cpu_type);
		if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) { stop_reason = 2; break; }

		if (breakpoint_hit(&breakpoints, cpu_ptr)) { 
            printf("STOP at $%04X\n", cpu_ptr->pc); 
            stop_reason = 3; break; 
        }

		if (trace_info.enabled) { trace_instruction_full(&trace_info, cpu_ptr, te->mnemonic, mode_name(te->mode), cpu_ptr->cycles); }
		execute_from_mem(cpu_ptr, cpu_ptr->mem, &dt, cpu_type);

        /* Throttling */
        if (speed_scale > 0.0f && ((cpu_ptr->cycles - cyc0) & 0x3FF) < 8) {
            struct timespec tnow; clock_gettime(CLOCK_MONOTONIC, &tnow);
            double elapsed = (tnow.tv_sec - t0.tv_sec) + (tnow.tv_nsec - t0.tv_nsec) * 1e-9;
            double target  = (double)(cpu_ptr->cycles - cyc0) / (C64_HZ * (double)speed_scale);
            if (target > elapsed) {
                double d = target - elapsed;
                struct timespec ts = { (time_t)d, (long)((d - (time_t)d) * 1e9) };
                nanosleep(&ts, NULL);
            }
        }
	}

    switch(stop_reason) {
        case 0: printf("\nExecution Stopped: Cycle limit (%lu) reached. Use '-L <n>' or '-c run' for longer execution.\n", cycle_limit); break;
        case 1: printf("\nExecution Finished: BRK encountered.\n"); break;
        case 2: printf("\nExecution Finished: STP encountered.\n"); break;
        default: printf("\nExecution Finished.\n"); break;
    }

	if (cpu_type == CPU_45GS02)
		printf("Registers: A=%02X X=%02X Y=%02X Z=%02X B=%02X S=%02X PC=%04X\n", cpu_ptr->a, cpu_ptr->x, cpu_ptr->y, cpu_ptr->z, cpu_ptr->b, (uint8_t)cpu_ptr->s, cpu_ptr->pc);
	else
		printf("Registers: A=%02X X=%02X Y=%02X S=%02X PC=%04X\n", cpu_ptr->a, cpu_ptr->x, cpu_ptr->y, (uint8_t)cpu_ptr->s, cpu_ptr->pc);

    if (show_memory) memory_dump(&mem, mem_start, mem_end);
	if (show_symbols) symbol_display(&symbols);

    /* Allow audio to drain */
    usleep(100000); 

	delete cpu_ptr;
	if (mem.io_registry) delete mem.io_registry;
    for (auto h : g_main_dynamic_handlers) delete h;
    audio_close();

	return 0;
}
