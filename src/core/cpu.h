#ifndef CPU_H
#define CPU_H

typedef enum {
	CPU_6502,
	CPU_6502_UNDOCUMENTED,
	CPU_65C02,
	CPU_65CE02,
	CPU_45GS02
} cpu_type_t;

#define FAR_PAGE_SHIFT  12
#define FAR_PAGE_SIZE   (1 << FAR_PAGE_SHIFT)		/* 4096 bytes per page */
#define FAR_NUM_PAGES   (0x10000000 >> FAR_PAGE_SHIFT)	/* 65536 pages for 28-bit space */

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

struct memory_t {
	unsigned char mem[0x10000];
	int mem_writes;
	unsigned short mem_addr[256];
	unsigned char mem_val[256];     /* value AFTER write  */
	unsigned char mem_old_val[256]; /* value BEFORE write */
	unsigned char *far_pages[FAR_NUM_PAGES];	/* sparse 28-bit page table */
	unsigned int map_offset[8];		/* MAP: per-8KB-block physical offset added to virtual addr; 0 = passthrough */
	IOHandler *io_handlers[0x10000];
	IORegistry *io_registry;
};

typedef struct memory_t memory_t;

/* CPUState: Non-abstract POD-like state for snapshots/history */
class CPUState {
public:
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
	memory_t *mem;

	CPUState() : mem(nullptr) { reset(); }
	virtual ~CPUState() {}

	virtual void reset() {
		a = 0; x = 0; y = 0; z = 0; b = 0;
		s = 0xFF; pc = 0; p = 0;
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
};

typedef CPUState cpu_t;

/* CPU: Abstract base class for execution */
class CPU : public CPUState {
public:
	virtual int step() = 0; /* Returns number of cycles executed */
	virtual void trigger_interrupt(int vector_addr) = 0;
	virtual void* get_interrupt_controller() = 0;

	CPU& operator=(const CPUState& other) {
		a = other.a; x = other.x; y = other.y; z = other.z; b = other.b;
		s = other.s; pc = other.pc; p = other.p;
		cycles = other.cycles; eom_prefix = other.eom_prefix;
		return *this;
	}
};

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
	cpu->reset();
}

static inline void set_flag(cpu_t *cpu, unsigned char flag, int set) {
	cpu->set_flag(flag, set);
}

static inline int get_flag(cpu_t *cpu, unsigned char flag) {
	return cpu->get_flag(flag);
}

static inline void update_nz(cpu_t *cpu, unsigned char val) {
	cpu->update_nz(val);
}

static inline void do_adc(cpu_t *cpu, unsigned char val) {
	cpu->do_adc(val);
}

static inline void do_sbc(cpu_t *cpu, unsigned char val) {
	cpu->do_sbc(val);
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
