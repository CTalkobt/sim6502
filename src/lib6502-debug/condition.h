#ifndef CONDITION_H
#define CONDITION_H

#include "cpu.h"

/* --- Expression Evaluator --- */

/* Parse a $hex, %binary, #decimal, or default hex token. */
int parse_mon_value(const char **pp, unsigned long *out);

/* Get value of a register or flag by name. */
unsigned long get_reg_val(const char *name, cpu_t *cpu);

/* Evaluate a simple condition string (e.g. "PC == $1234 && A == $00"). */
int evaluate_condition(const char *cond, cpu_t *cpu);

#endif
