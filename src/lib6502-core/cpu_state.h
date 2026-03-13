#ifndef CPU_STATE_H
#define CPU_STATE_H

#include <stdint.h>

/* CPUState: Pure POD struct for CPU register state. */
struct CPUState {
	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint8_t z;      /* 45GS02 only */
	uint8_t b;      /* 45GS02 only */
	uint16_t s;     /* 16-bit on 45GS02; 8-bit (page 1) on others */
	uint16_t pc;
	uint8_t p;
	uint64_t cycles;  /* Clock cycles executed */
	uint8_t eom_prefix; /* 45GS02: 1=EOM seen, 2=active flat sentinel */
};

#endif
