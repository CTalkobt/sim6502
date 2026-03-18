#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include "condition.h"
#include "cpu.h"
#include "memory.h"
#include "dispatch.h"
#include "disassembler.h"
#include "breakpoints.h"
#include "symbols.h"
#include <vector>
#include <string>

// Returns alphabetically sorted list of all command names that start with
// 'prefix'. Combines the dynamic CommandRegistry with all hardcoded commands.
std::vector<std::string> cli_get_completions(const char *prefix);

/* --- CLI Utilities --- */

extern int g_json_mode;
void json_err(const char *cmd, const char *msg);

#include "sim_api.h"

/* Enable JSON output mode for all interactive commands (0=text, 1=JSON) */
void cli_set_json_mode(int v);

void list_processors();

/* --- Interactive Modes --- */

bool cli_process_command(const std::string& line,
                        CPU *cpu, memory_t *mem,
                        cpu_type_t *p_cpu_type,
                        breakpoint_list_t *breakpoints,
                        symbol_table_t *symbols);

void run_interactive_mode(cpu_t *cpu, memory_t *mem,
                          cpu_type_t *p_cpu_type,
                          unsigned short start_addr, breakpoint_list_t *breakpoints,
                          symbol_table_t *symbols,
                          const std::vector<std::string>& initial_cmds);

/* --- Information Display --- */

void print_help(const char *progname);
void print_detailed_help(const char *cmd);
void list_processors(void);
void list_opcodes(cpu_type_t type);
void print_opcode_info(cpu_type_t cpu_type, const char *mnemonic);

#endif
