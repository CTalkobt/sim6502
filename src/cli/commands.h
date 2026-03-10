#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include "condition.h"
#include "cpu.h"
#include "memory.h"
#include "assembler.h"
#include "disassembler.h"
#include "breakpoints.h"
#include "symbols.h"

/* --- CLI Utilities --- */

/* Enable JSON output mode for all interactive commands (0=text, 1=JSON) */
void cli_set_json_mode(int v);

/* --- Interactive Modes --- */

void run_interactive_mode(cpu_t *cpu, memory_t *mem, 
                          opcode_handler_t **p_handlers, int *p_num_handlers,
                          cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                          unsigned short start_addr, breakpoint_list_t *breakpoints,
                          symbol_table_t *symbols,
                          const char *initial_cmd);

void run_asm_mode(memory_t *mem, symbol_table_t *symbols,
                  opcode_handler_t *handlers, int num_handlers,
                  cpu_type_t cpu_type, int *asm_pc);

/* --- Information Display --- */

void print_help(const char *progname);
void list_processors(void);
void list_opcodes(cpu_type_t type);
void print_opcode_info(opcode_handler_t *handlers, int num_handlers, const char *mnemonic);

#endif
