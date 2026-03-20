/*
 * sim_api.h — Public C++ API for the sim6502 simulator core.
 */

#ifndef SIM_API_H
#define SIM_API_H

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string>
#include "cpu.h"
#include "memory.h"
#include "machine.h"
#include "debug_types.h"

/* --------------------------------------------------------------------------
 * Execution state machine
 * -------------------------------------------------------------------------- */
typedef enum {
    SIM_IDLE,      /* no program loaded                                    */
    SIM_READY,     /* program loaded, not yet stepped                      */
    SIM_PAUSED,    /* stepped at least once; stopped (breakpoint/manual)   */
    SIM_FINISHED,  /* program returned (top-level RTS), STP, or cycle limit */
} sim_state_t;

/* Event codes returned by sim_step() and passed to sim_event_cb */
#define SIM_EVENT_BREAK  1   /* execution stopped at a breakpoint            */
#define SIM_EVENT_BRK    2   /* program returned via top-level RTS            */
#define SIM_EVENT_STP    3   /* STP instruction reached                      */
#define SIM_EVENT_STEP   4   /* single step completed (informational)        */

/* --------------------------------------------------------------------------
 * Opaque session handle
 * -------------------------------------------------------------------------- */
typedef struct sim_session sim_session_t;

/* Event callback: fired on stop conditions */
typedef void (*sim_event_cb)(sim_session_t *s, int event, void *userdata);

/* Lifecycle */
sim_session_t *sim_create(const char *processor);
void sim_destroy(sim_session_t *s);

/* Program loading */
int sim_load_asm(sim_session_t *s, const char *path);
int sim_load_bin(sim_session_t *s, const char *path, uint16_t load_addr);
int sim_load_prg(sim_session_t *s, const char *path, uint16_t override_addr);

/* Binary save */
int sim_save_bin(sim_session_t *s, const char *path, uint16_t addr_start, uint16_t count);
int sim_save_prg(sim_session_t *s, const char *path, uint16_t addr_start, uint16_t count);
void sim_get_load_info(sim_session_t *s, uint16_t *addr_out, uint16_t *size_out);

/* Execution control */
int sim_step(sim_session_t *s, int count);
int sim_step_over(sim_session_t *s);
int sim_step_out(sim_session_t *s);
int sim_step_cycles(sim_session_t *s, unsigned long max_cycles);
void sim_reset(sim_session_t *s);
void sim_clear_cycles(sim_session_t *s);

/* Disassembler */
typedef struct {
    unsigned short address;
    int            size;        /* total bytes consumed (prefix + opcode + operand) */
    char           bytes[24];   /* hex byte string, e.g. "A9 42" */
    char           mnemonic[8];
    char           operand[32]; /* e.g. "#$42", "$0300,X", "" for implied */
    int            cycles;
    uint32_t       target_addr; /* resolved memory address if applicable */
    bool           has_target;  /* true if target_addr is a valid memory address */
} sim_disasm_entry_t;

int sim_disassemble_one(sim_session_t *s, uint16_t addr, char *buf, size_t len);
int sim_disassemble_entry(sim_session_t *s, uint16_t addr, sim_disasm_entry_t *out);

/* State inspection */
CPU *sim_get_cpu(sim_session_t *s);
const memory_t *sim_get_memory(sim_session_t *s);
uint8_t sim_mem_read_byte(sim_session_t *s, uint16_t addr);
void    sim_mem_write_byte(sim_session_t *s, uint16_t addr, uint8_t val);

/* Session metadata */
const char  *sim_get_version(void);
sim_state_t  sim_get_state(sim_session_t *s);
void         sim_set_state(sim_session_t *s, sim_state_t state);
const char  *sim_get_filename(sim_session_t *s);
const char  *sim_get_last_error(sim_session_t *s);
const char  *sim_processor_name(sim_session_t *s);
const char  *sim_state_name(sim_state_t state);
void sim_set_processor(sim_session_t *s, const char *name);
cpu_type_t sim_get_cpu_type(sim_session_t *s);
void sim_set_debug(sim_session_t *s, bool debug);

/* Machine state */
machine_type_t sim_get_machine_type(sim_session_t *s);
void sim_set_machine_type(sim_session_t *s, machine_type_t machine);
const char *sim_machine_name(machine_type_t type);
int sim_device_add(sim_session_t *s, const char *name, uint16_t address);
int sim_get_device_count(sim_session_t *s);
int sim_get_device_info(sim_session_t *s, int idx, char *name_out, int name_sz, uint16_t *start_out, uint16_t *end_out);

/* Snippets (Idioms) */
typedef struct {
    const char *name;
    const char *category;
    const char *summary;
    const char *processor;
    const char *requires_device;
    const char *body;
} sim_snippet_t;

int sim_snippet_count(void);
int sim_snippet_get(int idx, sim_snippet_t *out);
int sim_snippet_find(const char *name, sim_snippet_t *out);

/* Event callbacks */
void sim_set_event_callback(sim_session_t *s, sim_event_cb cb, void *userdata);

/* Breakpoints */
int  sim_break_set(sim_session_t *s, uint16_t addr, const char *cond);
void sim_break_clear(sim_session_t *s, uint16_t addr);
int sim_break_is_enabled(sim_session_t *s, int idx);
int sim_break_toggle(sim_session_t *s, int idx);
int sim_break_count(sim_session_t *s);
int sim_break_get(sim_session_t *s, int idx, uint16_t *addr, char *cond, int cond_sz);
int sim_has_breakpoint(sim_session_t *s, uint16_t addr);

/* Symbol table */
const char *sim_sym_by_addr(sim_session_t *s, uint16_t addr);
int sim_sym_count(sim_session_t *s);
int sim_sym_get_idx(sim_session_t *s, int idx, uint16_t *addr, char *name_buf, int name_sz, int *type_out, char *comment_buf, int comment_sz);
const char *sim_sym_type_name(int type);
int sim_sym_remove_idx(sim_session_t *s, int idx);
int sim_sym_rename(sim_session_t *s, int idx, const char *new_name);
int sim_sym_set_addr(sim_session_t *s, int idx, uint16_t addr);
int sim_sym_add(sim_session_t *s, uint16_t addr, const char *name, const char *type_str);
int sim_sym_load_file(sim_session_t *s, const char *path);
int sim_sym_save_file(sim_session_t *s, const char *path);

/* Source Map */
int sim_source_lookup_addr(sim_session_t *s, uint16_t addr, char *path_out, int *line_out);
int sim_source_lookup_line(sim_session_t *s, const char *path, int line, uint16_t *addr_out);

/* Hardware Rendering */
void sim_vic_render_framebuffer(sim_session_t *s, uint8_t *buf);
void sim_vic_render_active_framebuffer(sim_session_t *s, uint8_t *buf);
void sim_vic_render_sprite(sim_session_t *s, int index, uint8_t *buf);
void sim_vic_render_char(sim_session_t *s, uint16_t char_base, int char_index, int mcm, uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t *buf);

/* Trace — types defined in debug_types.h */

void sim_trace_enable(sim_session_t *s, int enable);
int sim_trace_is_enabled(sim_session_t *s);
void sim_trace_clear(sim_session_t *s);
int sim_trace_count(sim_session_t *s);
uint64_t sim_trace_total_count(sim_session_t *s);
int sim_trace_get(sim_session_t *s, int slot, sim_trace_entry_t *entry);
int sim_trace_run(sim_session_t *s, int start_addr, int max_instr, int stop_on_brk, sim_trace_entry_t *entries, int entries_cap, char *stop_reason_out, int stop_reason_sz);

/* Register / State control */
typedef void (*sim_log_cb)(const char *text, void *userdata);
typedef void (*cli_log_cb)(const char *text, void *userdata);

void sim_set_log_callback(sim_session_t *s, sim_log_cb cb, void *userdata);
void sim_exec_command(sim_session_t *s, const char *cmd);
// Returns alphabetically sorted command names that begin with 'prefix'.
std::vector<std::string> sim_get_completions(const char *prefix);
// Returns alphabetically sorted symbol names (labels + constants) matching 'prefix'.
std::vector<std::string> sim_get_symbol_completions(sim_session_t *s, const char *prefix);
void cli_printf(const char *fmt, ...);
void cli_set_log_callback(cli_log_cb cb, void *userdata);
int  cliIsInteractiveMode(void);

void sim_set_pc(sim_session_t *s, uint16_t pc);
void sim_set_reg_byte(sim_session_t *s, const char *name, uint8_t val);
void sim_set_reg_value(sim_session_t *s, const char *name, uint16_t val);
int sim_get_last_writes(sim_session_t *s, uint16_t *addrs, int max_count);

/* History — types defined in debug_types.h */

void sim_history_enable(sim_session_t *s, int enable);
int  sim_history_is_enabled(sim_session_t *s);
void sim_history_clear(sim_session_t *s);
int  sim_history_depth(sim_session_t *s);
int  sim_history_count(sim_session_t *s);
int  sim_history_position(sim_session_t *s);
int sim_history_step_back(sim_session_t *s);
int sim_history_step_fwd(sim_session_t *s);
int sim_history_get(sim_session_t *s, int slot, sim_history_entry_t *entry);

/* Snapshots — types defined in debug_types.h */

void sim_snapshot_take(sim_session_t *s);
void sim_snapshot_clear(sim_session_t *s);
int  sim_snapshot_valid(sim_session_t *s);
int  sim_snapshot_diff(sim_session_t *s, sim_diff_entry_t *entries, int entries_cap);
uint64_t sim_snapshot_cycles(sim_session_t *s);
uint32_t sim_snapshot_timestamp(sim_session_t *s);

/* Validation */
#define SIM_VALIDATE_MEM_OPS 8
typedef struct {
    int      a, x, y, z, b, s, p;
    uint16_t mem_addr[SIM_VALIDATE_MEM_OPS];
    uint8_t  mem_val [SIM_VALIDATE_MEM_OPS];
    int      mem_count;
    char     label[48];
} sim_test_in_t;

typedef struct {
    int      a, x, y, z, b, s, p;
    uint16_t mem_addr[SIM_VALIDATE_MEM_OPS];
    uint8_t  mem_val [SIM_VALIDATE_MEM_OPS];
    int      mem_count;
} sim_test_expect_t;

typedef struct {
    int  passed;
    int  a, x, y, z, b, s, p;
    char fail_msg[128];
} sim_test_result_t;

int sim_validate_routine(sim_session_t *s, uint16_t routine_addr, uint16_t scratch_addr, int max_steps, const sim_test_in_t *inputs, const sim_test_expect_t *expects, sim_test_result_t *results, int count);

/* Profiler */
void sim_profiler_enable(sim_session_t *s, int enable);
int sim_profiler_is_enabled(sim_session_t *s);
void sim_profiler_clear(sim_session_t *s);
uint32_t sim_profiler_get_exec(sim_session_t *s, uint16_t addr);
uint32_t sim_profiler_get_cycles(sim_session_t *s, uint16_t addr);

/* Opcodes */
typedef struct {
    char          mnemonic[8];
    unsigned char mode;
    unsigned char opcode_bytes[4];
    unsigned char opcode_len;
    int           cycles;
    int           instr_bytes;
} sim_opcode_info_t;

const char *sim_mode_name(unsigned char mode);
int sim_opcode_count(sim_session_t *s);
int sim_opcode_get(sim_session_t *s, int idx, sim_opcode_info_t *info);

#endif /* SIM_API_H */
