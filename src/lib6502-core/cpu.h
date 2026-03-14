#ifndef CPU_H
#define CPU_H

#include "cpu_types.h"
#include "cpu_state.h"
#include "memory_types.h"
#include "dispatch.h"
#include "cpu_observer.h"

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20  /* Unused/always-1 bit (6502/65c02/65ce02) */
#define FLAG_E 0x20  /* Emulation flag — same bit, 65CE02/45GS02 context */
#define FLAG_V 0x40
#define FLAG_N 0x80

class IOHandler;
class IORegistry;

/* CPU: Abstract base class for execution, incorporating the state. */
class CPU : public CPUState {
public:
	bool debug;
	memory_t *mem;
	ExecutionObserver *observer = nullptr;

	CPU() : debug(false), mem(nullptr) { reset(); }
	virtual ~CPU() {}

	virtual void reset() {
		a = 0; x = 0; y = 0; z = 0; b = 0;
		s = 0xFF; pc = 0; p = FLAG_I | FLAG_U;
		cycles = 0; eom_prefix = 0;
	}

	void set_flag(unsigned char flag, int set) {
		if (set) p |= flag;
		else     p &= ~flag;
	}

	int get_flag(unsigned char flag) const {
		return (p & flag) ? 1 : 0;
	}

	void update_nz(unsigned char val) {
		set_flag(FLAG_Z, val == 0);
		set_flag(FLAG_N, val & 0x80);
	}

	void do_adc(unsigned char val) {
		if (get_flag(FLAG_D)) {
			int lo = (a & 0x0F) + (val & 0x0F) + get_flag(FLAG_C);
			int hi = (a >> 4) + (val >> 4);
			if (lo > 9) { lo -= 10; hi++; }
			int bin = a + val + get_flag(FLAG_C);
			set_flag(FLAG_V, ((a ^ bin) & (val ^ bin) & 0x80) != 0);
			if (hi > 9) { hi -= 10; set_flag(FLAG_C, 1); }
			else        {            set_flag(FLAG_C, 0); }
			a = (unsigned char)(((hi & 0x0F) << 4) | (lo & 0x0F));
			update_nz(a);
		} else {
			int result = a + val + get_flag(FLAG_C);
			set_flag(FLAG_C, result > 0xFF);
			set_flag(FLAG_V, ((a ^ result) & (val ^ result) & 0x80) != 0);
			a = result & 0xFF;
			update_nz(a);
		}
	}

	void do_sbc(unsigned char val) {
		if (get_flag(FLAG_D)) {
			int borrow = 1 - get_flag(FLAG_C);
			int lo = (a & 0x0F) - (val & 0x0F) - borrow;
			int hi = (a >> 4) - (val >> 4);
			if (lo < 0) { lo += 10; hi--; }
			int bin = a - val - borrow;
			set_flag(FLAG_C, bin >= 0);
			set_flag(FLAG_V, ((a ^ bin) & (~val ^ bin) & 0x80) != 0);
			if (hi < 0) hi += 10;
			a = (unsigned char)(((hi & 0x0F) << 4) | (lo & 0x0F));
			update_nz(a);
		} else {
			int result = a - val - (1 - get_flag(FLAG_C));
			set_flag(FLAG_C, result >= 0);
			set_flag(FLAG_V, ((a ^ result) & (~val ^ result) & 0x80) != 0);
			a = result & 0xFF;
			update_nz(a);
		}
	}

	virtual int step() = 0; /* Returns number of cycles executed */
	virtual void trigger_interrupt(int vector_addr) = 0;
	virtual void* get_interrupt_controller() = 0;
	virtual dispatch_table_t* dispatch_table() = 0;

	CPU& operator=(const CPUState& other) {
		a = other.a; x = other.x; y = other.y; z = other.z; b = other.b;
		s = other.s; pc = other.pc; p = other.p;
		cycles = other.cycles; eom_prefix = other.eom_prefix;
		return *this;
	}
};

typedef CPU cpu_t;

#define MODE_IMPLIED 0
#define MODE_IMMEDIATE 1
#define MODE_ABSOLUTE 2
#define MODE_ABSOLUTE_X 3
#define MODE_ABSOLUTE_Y 4
#define MODE_INDIRECT 5
#define MODE_INDIRECT_X 6
#define MODE_INDIRECT_Y 7
#define MODE_ZP 8
#define MODE_ZP_X 9
#define MODE_ZP_Y 10
#define MODE_RELATIVE 11
#define MODE_ZP_INDIRECT 12
#define MODE_ABS_INDIRECT_Y 13
#define MODE_RELATIVE_LONG 14
#define MODE_ZP_INDIRECT_Z 15
#define MODE_SP_INDIRECT_Y 16
#define MODE_ABS_INDIRECT_X 17
#define MODE_IMMEDIATE_WORD 18
#define MODE_ZP_INDIRECT_FLAT   19  /* [bp]   — flat (32-bit) ZP indirect, no Z offset */
#define MODE_ZP_INDIRECT_Z_FLAT 20  /* [bp],Z — flat (32-bit) ZP indirect, Z-indexed */

/* Cycle timing tables - base cycle counts per instruction */
/* All processors support these base cycles */
#define CYC_IMPLIED_BASE 2
#define CYC_IMMEDIATE_BASE 2
#define CYC_ABSOLUTE_BASE 4
#define CYC_ABSOLUTE_X_BASE 4
#define CYC_ABSOLUTE_Y_BASE 4
#define CYC_INDIRECT_BASE 6
#define CYC_INDIRECT_X_BASE 6
#define CYC_INDIRECT_Y_BASE 5
#define CYC_ZP_BASE 3
#define CYC_ZP_X_BASE 4
#define CYC_ZP_Y_BASE 4
#define CYC_RELATIVE_BASE 2
#define CYC_ZP_INDIRECT_BASE 5      /* 65CE02+ */
#define CYC_ABS_INDIRECT_Y_BASE 6   /* 65CE02+ */

/* Page boundary crossing penalties */
#define CYC_PAGE_CROSS 1

#endif

