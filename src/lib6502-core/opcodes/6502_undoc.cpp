#include "opcodes.h"

static void lax_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->a = val;
	cpu->x = val;
	cpu->update_nz(val);
	cpu->pc += 3;
}

static void lax_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->y);
	cpu->a = val;
	cpu->x = val;
	cpu->update_nz(val);
	cpu->pc += 3;
}

static void lax_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->a = val;
	cpu->x = val;
	cpu->update_nz(val);
	cpu->pc += 2;
}

static void lax_zp_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->y) & 0xFF);
	cpu->a = val;
	cpu->x = val;
	cpu->update_nz(val);
	cpu->pc += 2;
}

static void lax_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr + cpu->y);
	cpu->a = val;
	cpu->x = val;
	cpu->update_nz(val);
	cpu->pc += 2;
}

static void sax_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->a & cpu->x);
	cpu->pc += 3;
}

static void sax_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->a & cpu->x);
	cpu->pc += 2;
}

static void sax_zp_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->y) & 0xFF, cpu->a & cpu->x);
	cpu->pc += 2;
}

static void sax_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	mem_write(mem, addr, cpu->a & cpu->x);
	cpu->pc += 2;
}

static void dcm_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg) - 1) & 0xFF;
	mem_write(mem, arg, val);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->pc += 3;
}

static void dcm_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) - 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->pc += 2;
}

static void ins_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg) + 1) & 0xFF;
	mem_write(mem, arg, val);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->pc += 3;
}

static void ins_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) + 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->pc += 2;
}

static void arr_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
    (void)mem;
	cpu->a &= arg & 0xFF;
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a = ((cpu->a >> 1) | (c << 7)) & 0xFF;
	cpu->set_flag(FLAG_V, ((cpu->a >> 6) ^ (cpu->a >> 5)) & 1);
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void alr_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
    (void)mem;
	cpu->a &= arg & 0xFF;
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a >>= 1;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void anc_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
    (void)mem;
	cpu->a &= arg & 0xFF;
	cpu->set_flag(FLAG_C, cpu->a & 0x80);
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void slo_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 3;
}

static void slo_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg + cpu->x, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 3;
}

static void slo_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->y);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg + cpu->y, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 3;
}

static void slo_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void slo_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void slo_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void slo_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr + cpu->y);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, addr + cpu->y, val);
	cpu->a |= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void sre_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg, val);
	cpu->a ^= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 3;
}

static void sre_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg & 0xFF, val);
	cpu->a ^= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void rla_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg, val);
	cpu->a &= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 3;
}

static void rla_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->a &= val;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void rra_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg, val);
	int result = cpu->a + val + cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, result > 0xFF);
	cpu->set_flag(FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->pc += 3;
}

static void rra_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	int result = cpu->a + val + cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, result > 0xFF);
	cpu->set_flag(FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

opcode_handler_t opcodes_6502_undoc[] = {
	{"LAX", MODE_ABSOLUTE, lax_abs, 4, 0, 0, 0, {0xAF}, 1},
	{"LAX", MODE_ABSOLUTE_Y, lax_abs_y, 4, 0, 0, 0, {0xBF}, 1},
	{"LAX", MODE_ZP, lax_zp, 3, 0, 0, 0, {0xA7}, 1},
	{"LAX", MODE_ZP_Y, lax_zp_y, 4, 0, 0, 0, {0xB7}, 1},
	{"LAX", MODE_INDIRECT_Y, lax_ind_y, 5, 0, 0, 0, {0xB3}, 1},
	{"SAX", MODE_ABSOLUTE, sax_abs, 4, 0, 0, 0, {0x8F}, 1},
	{"SAX", MODE_ZP, sax_zp, 3, 0, 0, 0, {0x87}, 1},
	{"SAX", MODE_ZP_Y, sax_zp_y, 4, 0, 0, 0, {0x97}, 1},
	{"SAX", MODE_INDIRECT_X, sax_ind_x, 6, 0, 0, 0, {0x83}, 1},
	{"DCM", MODE_ABSOLUTE, dcm_abs, 6, 0, 0, 0, {0xCF}, 1},
	{"DCM", MODE_ZP, dcm_zp, 5, 0, 0, 0, {0xC7}, 1},
	{"INS", MODE_ABSOLUTE, ins_abs, 6, 0, 0, 0, {0xEF}, 1},
	{"INS", MODE_ZP, ins_zp, 5, 0, 0, 0, {0xE7}, 1},
	{"ARR", MODE_IMMEDIATE, arr_imm, 2, 0, 0, 0, {0x6B}, 1},
	{"ALR", MODE_IMMEDIATE, alr_imm, 2, 0, 0, 0, {0x4B}, 1},
	{"ANC", MODE_IMMEDIATE, anc_imm, 2, 0, 0, 0, {0x0B}, 1},
	{"SLO", MODE_ABSOLUTE, slo_abs, 6, 0, 0, 0, {0x0F}, 1},
	{"SLO", MODE_ABSOLUTE_X, slo_abs_x, 7, 0, 0, 0, {0x1F}, 1},
	{"SLO", MODE_ABSOLUTE_Y, slo_abs_y, 7, 0, 0, 0, {0x1B}, 1},
	{"SLO", MODE_ZP, slo_zp, 5, 0, 0, 0, {0x07}, 1},
	{"SLO", MODE_ZP_X, slo_zp_x, 6, 0, 0, 0, {0x17}, 1},
	{"SLO", MODE_INDIRECT_X, slo_ind_x, 8, 0, 0, 0, {0x03}, 1},
	{"SLO", MODE_INDIRECT_Y, slo_ind_y, 8, 0, 0, 0, {0x13}, 1},
	{"SRE", MODE_ABSOLUTE, sre_abs, 6, 0, 0, 0, {0x4F}, 1},
	{"SRE", MODE_ZP, sre_zp, 5, 0, 0, 0, {0x47}, 1},
	{"RLA", MODE_ABSOLUTE, rla_abs, 6, 0, 0, 0, {0x2F}, 1},
	{"RLA", MODE_ZP, rla_zp, 5, 0, 0, 0, {0x27}, 1},
	{"RRA", MODE_ABSOLUTE, rra_abs, 6, 0, 0, 0, {0x6F}, 1},
	{"RRA", MODE_ZP, rra_zp, 5, 0, 0, 0, {0x67}, 1},
};

int OPCODES_6502_UNDOC_COUNT = sizeof(opcodes_6502_undoc) / sizeof(opcodes_6502_undoc[0]);
