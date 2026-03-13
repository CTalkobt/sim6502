#ifndef MACHINE_H
#define MACHINE_H

/* Machine (system) profiles — simulation-level concept, separate from CPU ISA variant. */
typedef enum {
	MACHINE_RAW6502,
	MACHINE_C64,
	MACHINE_C128,
	MACHINE_MEGA65,
	MACHINE_X16
} machine_type_t;

#endif
