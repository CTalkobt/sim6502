#ifndef CPU_H
#define CPU_H

typedef enum {
	CPU_6502,
	CPU_6502_UNDOCUMENTED,
	CPU_65C02,
	CPU_65CE02,
	CPU_45GS02
} cpu_type_t;

typedef struct {
	unsigned short address;
	int enabled;
	char condition[128];
} breakpoint_t;

typedef struct {
	unsigned char a;
	unsigned char x;
	unsigned char y;
	unsigned char z;      /* 45GS02 only */
	unsigned char b;      /* 45GS02 only */
	unsigned short s;     /* 16-bit on 45GS02; 8-bit (page 1) on others */
	unsigned short pc;
	unsigned char p;
	unsigned long cycles;  /* Clock cycles executed */
	unsigned char eom_prefix; /* 45GS02: 1=EOM seen, 2=active flat sentinel */
} cpu_t;

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20  /* Unused/always-1 bit (6502/65c02/65ce02) */
#define FLAG_E 0x20  /* Emulation flag — same bit, 65CE02/45GS02 context */
#define FLAG_V 0x40
#define FLAG_N 0x80

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

static inline void cpu_init(cpu_t *cpu) {
	cpu->a = 0;
	cpu->x = 0;
	cpu->y = 0;
	cpu->z = 0;
	cpu->b = 0;
	cpu->s = 0xFF;
	cpu->pc = 0;
	cpu->p = 0;
	cpu->cycles = 0;
	cpu->eom_prefix = 0;
}

static inline void set_flag(cpu_t *cpu, unsigned char flag, int set) {
	if (set)
		cpu->p |= flag;
	else
		cpu->p &= ~flag;
}

static inline int get_flag(cpu_t *cpu, unsigned char flag) {
	return (cpu->p & flag) ? 1 : 0;
}

static inline void update_nz(cpu_t *cpu, unsigned char val) {
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x80);
}

static inline void do_adc(cpu_t *cpu, unsigned char val) {
	if (get_flag(cpu, FLAG_D)) {
		int lo = (cpu->a & 0x0F) + (val & 0x0F) + get_flag(cpu, FLAG_C);
		int hi = (cpu->a >> 4) + (val >> 4);
		if (lo > 9) { lo -= 10; hi++; }
		int bin = cpu->a + val + get_flag(cpu, FLAG_C);
		set_flag(cpu, FLAG_V, ((cpu->a ^ bin) & (val ^ bin) & 0x80) != 0);
		if (hi > 9) { hi -= 10; set_flag(cpu, FLAG_C, 1); }
		else        {            set_flag(cpu, FLAG_C, 0); }
		cpu->a = (unsigned char)(((hi & 0x0F) << 4) | (lo & 0x0F));
		update_nz(cpu, cpu->a);
	} else {
		int result = cpu->a + val + get_flag(cpu, FLAG_C);
		set_flag(cpu, FLAG_C, result > 0xFF);
		set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
		cpu->a = result & 0xFF;
		update_nz(cpu, cpu->a);
	}
}

static inline void do_sbc(cpu_t *cpu, unsigned char val) {
	if (get_flag(cpu, FLAG_D)) {
		int borrow = 1 - get_flag(cpu, FLAG_C);
		int lo = (cpu->a & 0x0F) - (val & 0x0F) - borrow;
		int hi = (cpu->a >> 4) - (val >> 4);
		if (lo < 0) { lo += 10; hi--; }
		int bin = cpu->a - val - borrow;
		set_flag(cpu, FLAG_C, bin >= 0);
		set_flag(cpu, FLAG_V, ((cpu->a ^ bin) & (~val ^ bin) & 0x80) != 0);
		if (hi < 0) hi += 10;
		cpu->a = (unsigned char)(((hi & 0x0F) << 4) | (lo & 0x0F));
		update_nz(cpu, cpu->a);
	} else {
		int result = cpu->a - val - (1 - get_flag(cpu, FLAG_C));
		set_flag(cpu, FLAG_C, result >= 0);
		set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (~val ^ result) & 0x80) != 0);
		cpu->a = result & 0xFF;
		update_nz(cpu, cpu->a);
	}
}

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
