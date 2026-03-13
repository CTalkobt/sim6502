#ifndef DEBUG_TYPES_H
#define DEBUG_TYPES_H

#include <stdint.h>
#include "cpu_state.h"

/* Trace ring buffer depth */
#define SIM_TRACE_DEPTH 256

/* History ring buffer default depth */
#define SIM_HIST_DEFAULT_DEPTH (1 << 17)

/* Single trace entry */
typedef struct {
    uint16_t pc;
    char     disasm[64];
    CPUState cpu;
    int      cycles_delta;
} sim_trace_entry_t;

/* Single history entry */
typedef struct {
    CPUState pre_cpu;
    uint16_t pc;
    uint8_t  delta_count;
    uint16_t delta_addr[16];
    uint8_t  delta_old[16];
} sim_history_entry_t;

/* Single snapshot diff entry */
typedef struct {
    uint16_t addr;
    uint8_t  before;
    uint8_t  after;
    uint16_t writer_pc;
} sim_diff_entry_t;

#endif /* DEBUG_TYPES_H */
