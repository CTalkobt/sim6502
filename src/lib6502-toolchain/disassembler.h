#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stdint.h>
#include "cpu.h"
#include "memory.h"
#include "dispatch.h"

/* peek_dispatch: return the dispatch_entry_t for the instruction at cpu->pc. */
const dispatch_entry_t *peek_dispatch(const CPUState *cpu, const memory_t *mem, const dispatch_table_t *dt, cpu_type_t cpu_type);

/* --- Disassembler API --- */

/*
 * disasm_one: disassemble one instruction at 'addr'.
 * buf: output buffer
 * Returns the number of bytes consumed.
 */
int disasm_one(const memory_t *mem, const dispatch_table_t *dt,
               cpu_type_t cpu_type, unsigned short addr,
               char *buf, int bufsz);

/*
 * disasm_entry_t: structured result of disassembling one instruction.
 * Filled by disasm_one_entry().
 */
typedef struct {
    unsigned short address;
    int            size;        /* total bytes consumed (prefix + opcode + operand) */
    char           bytes[24];   /* hex byte string, e.g. "A9 42" */
    char           mnemonic[8];
    char           operand[32]; /* e.g. "#$42", "$0300,X", "" for implied */
    int            cycles;
    uint32_t       target_addr;
    bool           has_target;
} disasm_entry_t;

/*
 * disasm_one_entry: disassemble one instruction into a structured disasm_entry_t.
 * Returns the number of bytes consumed (same as disasm_one).
 */
int disasm_one_entry(const memory_t *mem, const dispatch_table_t *dt,
                     cpu_type_t cpu_type, unsigned short addr,
                     disasm_entry_t *out);

#endif
