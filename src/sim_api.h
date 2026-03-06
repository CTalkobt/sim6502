/*
 * sim_api.h — Public C API for the sim6502 simulator core.
 *
 * The simulator session is an opaque handle (sim_session_t *).  All mutable
 * state lives inside the session; the caller never touches internals.
 *
 * Thread-safety: Phase 1 is single-threaded.  The GUI drives execution by
 * calling sim_step() each frame.  Phase 2 will add a mutex-guarded background
 * thread; the API surface is the same.
 */

#ifndef SIM_API_H
#define SIM_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "cpu.h"

/* --------------------------------------------------------------------------
 * Execution state machine
 * -------------------------------------------------------------------------- */
typedef enum {
    SIM_IDLE,      /* no program loaded                                    */
    SIM_READY,     /* program loaded, not yet stepped                      */
    SIM_PAUSED,    /* stepped at least once; stopped (breakpoint/manual)   */
    SIM_FINISHED,  /* reached BRK, STP, or cycle limit                    */
} sim_state_t;

/* Event codes returned by sim_step() and passed to sim_event_cb */
#define SIM_EVENT_BREAK  1   /* execution stopped at a breakpoint            */
#define SIM_EVENT_BRK    2   /* BRK instruction reached                      */
#define SIM_EVENT_STP    3   /* STP instruction reached                      */
#define SIM_EVENT_STEP   4   /* single step completed (informational)        */

/* --------------------------------------------------------------------------
 * Opaque session handle
 * -------------------------------------------------------------------------- */
typedef struct sim_session sim_session_t;

/* Event callback: fired on stop conditions */
typedef void (*sim_event_cb)(sim_session_t *s, int event, void *userdata);

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/* Create a new session.  processor is "6502", "6502-undoc", "65c02",
 * "65ce02", or "45gs02" (NULL defaults to "6502").
 * Returns NULL on allocation failure.                                       */
sim_session_t *sim_create(const char *processor);

/* Destroy the session and free all memory. */
void sim_destroy(sim_session_t *s);

/* --------------------------------------------------------------------------
 * Program loading
 * -------------------------------------------------------------------------- */

/* Assemble 'path' (two-pass) and load it into the session.
 * Returns 0 on success, -1 on error (file not found, assemble failure).
 * On success, state transitions to SIM_READY.                              */
int sim_load_asm(sim_session_t *s, const char *path);

/* Load a raw binary file into memory at load_addr.
 * Resets memory, symbols, and breakpoints.  PC is set to load_addr.
 * Returns 0 on success, -1 on error (file not found, empty file).
 * On success, state transitions to SIM_READY.                              */
int sim_load_bin(sim_session_t *s, const char *path, uint16_t load_addr);

/* Load a .prg file (Commodore-style 2-byte LE load address header + data).
 * If override_addr != 0, use that instead of the embedded header address.
 * Returns 0 on success, -1 on error (file not found, header too short).
 * On success, state transitions to SIM_READY.                              */
int sim_load_prg(sim_session_t *s, const char *path, uint16_t override_addr);

/* --------------------------------------------------------------------------
 * Binary save
 * -------------------------------------------------------------------------- */

/* Write memory[addr_start .. addr_start+count-1] to a raw binary file.
 * Returns 0 on success, -1 on error.                                       */
int sim_save_bin(sim_session_t *s, const char *path,
                 uint16_t addr_start, uint16_t count);

/* Write a .prg file: 2-byte LE load-address header followed by
 * memory[addr_start .. addr_start+count-1].
 * Returns 0 on success, -1 on error.                                       */
int sim_save_prg(sim_session_t *s, const char *path,
                 uint16_t addr_start, uint16_t count);

/* Return the load address and byte count of the most recently loaded file.
 * For sim_load_asm this reflects the assembled code range.
 * Either pointer may be NULL.                                               */
void sim_get_load_info(sim_session_t *s, uint16_t *addr_out, uint16_t *size_out);

/* --------------------------------------------------------------------------
 * Execution control
 * -------------------------------------------------------------------------- */

/* Run up to 'count' instructions.
 * Returns 0 if all 'count' instructions ran without a stop condition.
 * Returns SIM_EVENT_* (> 0) if a stop condition was hit before count.
 * Returns -1 if state is SIM_IDLE or SIM_FINISHED (call sim_reset first).
 * On stop: state is set to SIM_PAUSED (breakpoint) or SIM_FINISHED (BRK/STP). */
int sim_step(sim_session_t *s, int count);

/* Reset CPU registers to initial state; keep loaded program in memory.
 * State transitions back to SIM_READY.                                     */
void sim_reset(sim_session_t *s);

/* --------------------------------------------------------------------------
 * Disassembler
 * -------------------------------------------------------------------------- */

/* Disassemble one instruction at 'addr' into buf.
 * Returns the number of bytes consumed (>= 1).                             */
int sim_disassemble_one(sim_session_t *s, uint16_t addr, char *buf, size_t len);

/* --------------------------------------------------------------------------
 * State inspection
 * -------------------------------------------------------------------------- */

/* Direct pointer to the session's cpu_t.  Valid until sim_destroy().
 * Read-only from the GUI; modify only through the API (future sim_set_reg). */
cpu_t *sim_get_cpu(sim_session_t *s);

/* Read / write a byte in the session's virtual address space. */
uint8_t sim_mem_read_byte(sim_session_t *s, uint16_t addr);
void    sim_mem_write_byte(sim_session_t *s, uint16_t addr, uint8_t val);

/* --------------------------------------------------------------------------
 * Session metadata
 * -------------------------------------------------------------------------- */

sim_state_t  sim_get_state(sim_session_t *s);
const char  *sim_get_filename(sim_session_t *s);
const char  *sim_processor_name(sim_session_t *s);
const char  *sim_state_name(sim_state_t state);

/* Change the active processor.  Safe to call before or after load.
 * If a program is already loaded, rebuild the dispatch table.              */
void sim_set_processor(sim_session_t *s, const char *name);

/* Return the raw cpu_type_t for the current processor. */
cpu_type_t sim_get_cpu_type(sim_session_t *s);

/* --------------------------------------------------------------------------
 * Event callbacks
 * -------------------------------------------------------------------------- */
void sim_set_event_callback(sim_session_t *s, sim_event_cb cb, void *userdata);

/* --------------------------------------------------------------------------
 * Breakpoints
 * -------------------------------------------------------------------------- */

/* Add a breakpoint at addr with optional condition string.
 * Returns 1 on success, 0 if the breakpoint table is full.                 */
int  sim_break_set(sim_session_t *s, uint16_t addr, const char *cond);

/* Remove the breakpoint at addr (no-op if not found). */
void sim_break_clear(sim_session_t *s, uint16_t addr);

/* --------------------------------------------------------------------------
 * Symbol table
 * -------------------------------------------------------------------------- */

/* Return the name of the symbol at addr, or NULL if none found. */
const char *sim_sym_by_addr(sim_session_t *s, uint16_t addr);

/* --------------------------------------------------------------------------
 * Phase 2 extensions
 * -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 * Phase 3 extensions
 * -------------------------------------------------------------------------- */

/* Returns 1 if the idx-th breakpoint is enabled, 0 if disabled or OOB.    */
int sim_break_is_enabled(sim_session_t *s, int idx);

/* Toggle the enabled flag for the idx-th breakpoint.
 * Returns the new state (1=enabled, 0=disabled), or -1 if OOB.            */
int sim_break_toggle(sim_session_t *s, int idx);

/* ---- Execution trace ---- */

#define SIM_TRACE_DEPTH 256

typedef struct {
    uint16_t pc;           /* instruction address (pre-execution)  */
    char     disasm[64];   /* full disasm string from disasm_one() */
    cpu_t    cpu;          /* CPU state AFTER execution             */
    int      cycles_delta; /* clock cycles consumed                 */
} sim_trace_entry_t;

/* Enable (enable=1) or disable (enable=0) per-instruction trace recording. */
void sim_trace_enable(sim_session_t *s, int enable);

/* Return 1 if trace recording is enabled, 0 otherwise. */
int sim_trace_is_enabled(sim_session_t *s);

/* Clear the trace ring buffer. */
void sim_trace_clear(sim_session_t *s);

/* Return the number of entries in the trace buffer (0..SIM_TRACE_DEPTH).   */
int sim_trace_count(sim_session_t *s);

/* Fill *entry for slot (0 = most recent, count-1 = oldest).
 * Returns 1 on success, 0 if slot is out of range.                        */
int sim_trace_get(sim_session_t *s, int slot, sim_trace_entry_t *entry);

/* Returns 1 if a breakpoint is set at addr, 0 otherwise. */
int sim_has_breakpoint(sim_session_t *s, uint16_t addr);

/* Return the cycle count for the instruction at addr according to the active
 * processor variant.  Returns 0 if addr holds an unrecognised opcode.      */
int sim_get_opcode_cycles(sim_session_t *s, uint16_t addr);

/* Copy up to max_count recently-written virtual addresses into addrs[].
 * Returns the actual count (≤ min(256, max_count)).
 * The write log is cleared at the start of every sim_step() call.          */
int sim_get_last_writes(sim_session_t *s, uint16_t *addrs, int max_count);

/* Set the program counter directly (safe between steps). */
void sim_set_pc(sim_session_t *s, uint16_t pc);

/* Set a CPU register by name: "A" "X" "Y" "Z" "B" "S" "P".
 * For S on non-45GS02: sets the low byte (stack page 1).
 * No-op for unrecognised names.                                             */
void sim_set_reg_byte(sim_session_t *s, const char *name, uint8_t val);

/* Return the number of active breakpoints. */
int sim_break_count(sim_session_t *s);

/* Fill *addr and cond[0..cond_sz-1] for the idx-th breakpoint.
 * Returns 1 on success, 0 if idx is out of range.  cond may be NULL.      */
int sim_break_get(sim_session_t *s, int idx, uint16_t *addr,
                  char *cond, int cond_sz);

/* --------------------------------------------------------------------------
 * Phase 4 extensions
 * -------------------------------------------------------------------------- */

/* ---- Opcode browser ---- */

typedef struct {
    char          mnemonic[8];
    unsigned char mode;
    unsigned char opcode_bytes[4];
    unsigned char opcode_len;
    int           cycles;       /* cycles for the active processor variant */
    int           instr_bytes;  /* total encoded bytes (prefix + operand)  */
} sim_opcode_info_t;

/* Return the addressing-mode name string for a MODE_* constant.             */
const char *sim_mode_name(unsigned char mode);

/* Number of opcodes in the active handler table.                            */
int sim_opcode_count(sim_session_t *s);

/* Fill *info for the idx-th handler entry. Returns 1 on success, 0 if OOB. */
int sim_opcode_get(sim_session_t *s, int idx, sim_opcode_info_t *info);

/* Fill *info for the single-byte opcode whose base byte == byte_val.
 * Returns 1 if found, 0 if no matching handler.                            */
int sim_opcode_by_byte(sim_session_t *s, uint8_t byte_val, sim_opcode_info_t *info);

/* ---- Symbol table browser ---- */

/* Number of symbols in the session's symbol table.                          */
int sim_sym_count(sim_session_t *s);

/* Fill details for the idx-th symbol.  name_buf / comment_buf may be NULL.
 * Returns 1 on success, 0 if idx is out of range.                          */
int sim_sym_get_idx(sim_session_t *s, int idx,
                    uint16_t *addr,
                    char *name_buf,    int name_sz,
                    int  *type_out,
                    char *comment_buf, int comment_sz);

/* Human-readable name for a symbol_type_t integer value.                   */
const char *sim_sym_type_name(int type);

/* Remove the idx-th symbol (remaining symbols shift down).
 * Returns 1 on success, 0 if OOB.                                          */
int sim_sym_remove_idx(sim_session_t *s, int idx);

/* Add a symbol.  type_str: "LABEL","VAR","CONST","FUNC","IO","REGION","TRAP".
 * Returns 1 on success, 0 if table full or duplicate name.                 */
int sim_sym_add(sim_session_t *s, uint16_t addr,
                const char *name, const char *type_str);

/* Load symbols from a .sym file into the session symbol table.
 * Returns the count of symbols successfully added.                          */
int sim_sym_load_file(sim_session_t *s, const char *path);

/* ---- Execution History ---- */

#define SIM_HIST_DEFAULT_DEPTH (1 << 17)   /* 131072 entries (~10 MB)       */

/* One history entry: pre-execution CPU snapshot + sparse memory deltas.    */
typedef struct {
    cpu_t    pre_cpu;           /* CPU state BEFORE executing the instruction */
    uint16_t pc;                /* PC before execution (== pre_cpu.pc)        */
    uint8_t  delta_count;       /* number of memory writes recorded (≤ 16)    */
    uint16_t delta_addr[16];    /* virtual addresses written                  */
    uint8_t  delta_old[16];     /* byte values BEFORE each write              */
} sim_history_entry_t;

/* Enable (1) or disable (0) history recording.
 * Disabling does not erase existing entries.                                */
void sim_history_enable(sim_session_t *s, int enable);

/* Return 1 if history recording is enabled, 0 otherwise.                   */
int sim_history_is_enabled(sim_session_t *s);

/* Clear all history entries; reset position to 0.                          */
void sim_history_clear(sim_session_t *s);

/* Return the ring buffer capacity (maximum entries storable).               */
int sim_history_depth(sim_session_t *s);

/* Return the number of entries currently stored (0 .. depth).               */
int sim_history_count(sim_session_t *s);

/* Return the current step-back position (0 = present, N = N steps back).   */
int sim_history_position(sim_session_t *s);

/* Step back one instruction: restore CPU and memory to the pre-execute state.
 * Returns 1 on success, 0 if no history remains.                           */
int sim_history_step_back(sim_session_t *s);

/* Step forward one instruction: re-execute the instruction that was undone.
 * Returns 1 on success, 0 if already at the present.                       */
int sim_history_step_fwd(sim_session_t *s);

/* Fill *entry for the slot-th history entry (0 = most recent).
 * Returns 1 on success, 0 if slot is out of range.                         */
int sim_history_get(sim_session_t *s, int slot, sim_history_entry_t *entry);

/* ---- Profiler ---- */

/* Enable (1) or disable (0) per-instruction profiling.                      */
void sim_profiler_enable(sim_session_t *s, int enable);

/* Return 1 if profiling is currently enabled, 0 otherwise.                  */
int sim_profiler_is_enabled(sim_session_t *s);

/* Reset all profiling counters to zero.                                     */
void sim_profiler_clear(sim_session_t *s);

/* Return the instruction execution count for addr.                          */
uint32_t sim_profiler_get_exec(sim_session_t *s, uint16_t addr);

/* Return the total cycles accumulated at addr.                              */
uint32_t sim_profiler_get_cycles(sim_session_t *s, uint16_t addr);

/* Fill out_addrs[] / out_counts[] with the top-N most-executed addresses,
 * sorted descending by execution count.  max_n is capped at 64 internally.
 * Returns the actual count filled (≤ max_n).                               */
int sim_profiler_top_exec(sim_session_t *s,
                          uint16_t *out_addrs, uint32_t *out_counts,
                          int max_n);

#ifdef __cplusplus
}
#endif

#endif /* SIM_API_H */
