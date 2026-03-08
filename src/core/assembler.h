#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "symbols.h"

struct source_stack;

/* instruction_t: Represents a parsed instruction. */
typedef struct {
	char op[8];
	unsigned char mode;
	unsigned short arg;
	unsigned char arg_overflow; /* set if literal value is out of range for the mode */
} instruction_t;

/* --- Core Assembler API --- */

/* 
 * handle_pseudo_op: process a assembler directive line.
 * line     — points at or before the leading '.' (leading whitespace is skipped)
 * cpu_type — updated by .processor
 * pc       — updated by .org / .byte / .word / .text / .align
 * mem      — if non-NULL (second pass) bytes are written; if NULL (first pass) only pc advances
 * symbols  — used in second pass to resolve label operands in .word/.byte; may be NULL
 * ss       — optional source stack for macros/includes/loops
 */
void handle_pseudo_op(const char *line, cpu_type_t *cpu_type, int *pc,
                      memory_t *mem, symbol_table_t *symbols, struct source_stack *ss);

/*
 * parse_line: parse a single line of assembly into an instruction_t.
 * line    — input source line
 * instr   — output parsed instruction (instr->op[0] == 0 if blank/comment)
 * symbols — used to resolve label operands (may be NULL)
 * pc      — current address (used to resolve relative branch targets)
 */
void parse_line(const char *line, instruction_t *instr, symbol_table_t *symbols, int pc);

/*
 * encode_to_mem: binary encode an instruction_t into memory.
 * mem      — destination memory
 * pc_base  — address to write at
 * instr    — instruction to encode
 * handlers — opcode handler table for the active CPU
 * n        — number of handlers in table
 * Returns the total number of bytes written, or -1 if not found.
 */
int encode_to_mem(memory_t *mem, int pc_base,
                  const instruction_t *instr,
                  const opcode_handler_t *handlers, int n,
                  cpu_type_t cpu_type);

/* --- Utility Functions --- */

/* convert a literal constant ($hex, %bin, 'c', dec) to a numeric value. */
unsigned short parse_value(const char *str, int *out_digits);

/* read a raw binary file into memory starting at addr. */
int load_binary_to_mem(memory_t *mem, int addr, const char *filename);

/* helper to determine instruction length based on addressing mode. */
int get_instruction_length(unsigned char mode);

/* return the total encoded byte count for (mnemonic, mode). */
int get_encoded_length(const char *mnem, unsigned char mode,
                       const opcode_handler_t *handlers, int n,
                       cpu_type_t cpu_type);

/* return 1 if the mnemonic is a branch instruction. */
int is_branch_opcode(const char *op);

/* return 1 if the mnemonic is a long branch instruction (45GS02). */
int is_long_branch_opcode(const char *op);

/* return the addressing-mode name string for a MODE_* constant. */
const char *mode_name(unsigned char mode);

#endif
