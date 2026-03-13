#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>
#include <string.h>
#include "cpu.h"

typedef struct {
	FILE *file;
	int enabled;
	int to_file;
	char filename[256];
} trace_t;

/* Initialize trace system */
static inline void trace_init(trace_t *trace) {
	trace->file = NULL;
	trace->enabled = 0;
	trace->to_file = 0;
	trace->filename[0] = 0;
}

/* Enable trace to stdout */
static inline void trace_enable_stdout(trace_t *trace) {
	trace->enabled = 1;
	trace->to_file = 0;
	trace->file = stdout;
}

/* Enable trace to file */
static inline int trace_enable_file(trace_t *trace, const char *filename) {
	FILE *f = fopen(filename, "w");
	if (!f) {
		return 0;
	}
	trace->enabled = 1;
	trace->to_file = 1;
	trace->file = f;
	strncpy(trace->filename, filename, sizeof(trace->filename) - 1);
	
	/* Write header */
	fprintf(f, "=== Execution Trace ===\n");
	fprintf(f, "PC   | OP  | Addr     | A  X  Y  S  P | Cycles\n");
	fprintf(f, "-----+-----+----------+---------------+-------\n");
	
	return 1;
}

/* Disable trace */
static inline void trace_disable(trace_t *trace) {
	if (trace->to_file && trace->file) {
		fclose(trace->file);
	}
	trace->enabled = 0;
	trace->file = NULL;
}

/* Log instruction execution */
static inline void trace_instruction(trace_t *trace, const cpu_t *cpu,
	const char *mnemonic, unsigned long cycles_before) {
	
	if (!trace->enabled || !trace->file) return;
	
	(void)cycles_before;
	
	fprintf(trace->file, "%04X | %s  | 0x0000   | %02X %02X %02X %02X %02X | %lu\n",
		cpu->pc,
		mnemonic,
		cpu->a, cpu->x, cpu->y, cpu->s, cpu->p,
		cpu->cycles);
}

/* Trace with more detail */
static inline void trace_instruction_full(trace_t *trace, const cpu_t *cpu,
	const char *mnemonic, const char *addr_mode, unsigned long cycles_before) {
	
	if (!trace->enabled || !trace->file) return;
	
	(void)cycles_before;
	
	fprintf(trace->file, "%04X | %-3s | %s | %02X %02X %02X %02X %02X | %lu\n",
		cpu->pc,
		mnemonic,
		addr_mode,
		cpu->a, cpu->x, cpu->y, cpu->s, cpu->p,
		cpu->cycles);
}

#endif
