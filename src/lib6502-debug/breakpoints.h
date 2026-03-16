#include "sim_api.h"
#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include "cpu.h"
#include "condition.h"
#include <stdio.h>
#include <string.h>

#define MAX_BREAKPOINTS 16

typedef struct {
	unsigned short address;
	int enabled;
	char condition[128];
} breakpoint_t;

typedef struct {
	breakpoint_t breakpoints[MAX_BREAKPOINTS];
	int count;
} breakpoint_list_t;

/* Initialize breakpoint system */
static inline void breakpoint_init(breakpoint_list_t *bp_list) {
	bp_list->count = 0;
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		bp_list->breakpoints[i].address = 0;
		bp_list->breakpoints[i].enabled = 0;
		bp_list->breakpoints[i].condition[0] = '\0';
	}
}

/* Add a breakpoint at address with optional condition */
static inline int breakpoint_add(breakpoint_list_t *bp_list, unsigned short address, const char *condition) {
	if (bp_list->count >= MAX_BREAKPOINTS) {
		return 0;  /* Too many breakpoints */
	}
	bp_list->breakpoints[bp_list->count].address = address;
	bp_list->breakpoints[bp_list->count].enabled = 1;
	if (condition) {
		strncpy(bp_list->breakpoints[bp_list->count].condition, condition, sizeof(bp_list->breakpoints[bp_list->count].condition) - 1);
		bp_list->breakpoints[bp_list->count].condition[sizeof(bp_list->breakpoints[bp_list->count].condition) - 1] = '\0';
	} else {
		bp_list->breakpoints[bp_list->count].condition[0] = '\0';
	}
	bp_list->count++;
	return 1;
}

/* Remove a breakpoint at address */
static inline int breakpoint_remove(breakpoint_list_t *bp_list, unsigned short address) {
	for (int i = 0; i < bp_list->count; i++) {
		if (bp_list->breakpoints[i].address == address) {
			/* Shift remaining breakpoints down */
			for (int j = i; j < bp_list->count - 1; j++) {
				bp_list->breakpoints[j] = bp_list->breakpoints[j+1];
			}
			bp_list->count--;
			return 1;
		}
	}
	return 0;
}

/* Check if PC has breakpoint and condition is met */
static inline int breakpoint_hit(breakpoint_list_t *bp_list, cpu_t *cpu) {
	for (int i = 0; i < bp_list->count; i++) {
		if (bp_list->breakpoints[i].enabled && 
		    bp_list->breakpoints[i].address == cpu->pc) {
			if (bp_list->breakpoints[i].condition[0] != '\0') {
				if (evaluate_condition(bp_list->breakpoints[i].condition, cpu)) {
					return 1;
				}
			} else {
				return 1;
			}
		}
	}
	return 0;
}

/* List all breakpoints */
static inline void breakpoint_list(breakpoint_list_t *bp_list) {
	if (bp_list->count == 0) {
		cli_printf("No breakpoints set.\n");
		return;
	}
	cli_printf("Breakpoints:\n");
	for (int i = 0; i < bp_list->count; i++) {
		cli_printf("  %d: 0x%04X %s%s%s\n", i, bp_list->breakpoints[i].address,
			bp_list->breakpoints[i].enabled ? "[enabled]" : "[disabled]",
			bp_list->breakpoints[i].condition[0] ? " if " : "",
			bp_list->breakpoints[i].condition);
	}
}

#endif
