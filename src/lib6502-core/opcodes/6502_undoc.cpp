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

static void asr_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= arg & 0xFF;
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a >>= 1;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void arr_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= arg & 0xFF;
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a = ((cpu->a >> 1) | (c << 7)) & 0xFF;
	cpu->set_flag(FLAG_V, ((cpu->a >> 6) ^ (cpu->a >> 5)) & 1);
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void alr_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= arg & 0xFF;
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a >>= 1;
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void anc_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
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

extern void lda_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_abs_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_abs_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_zp_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_ind_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lda_ind_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldx_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldx_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldx_abs_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldx_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldx_zp_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldy_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldy_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldy_abs_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldy_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ldy_zp_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_abs_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_abs_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_zp_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_ind_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sta_ind_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void stx_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void stx_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void stx_zp_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sty_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sty_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sty_zp_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_abs_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_abs_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_zp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_zp_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_ind_x(CPU *cpu, memory_t *mem, unsigned short arg);
extern void adc_ind_y(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sbc_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sbc_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void cmp_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void cmp_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void cpx_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void cpy_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void inc_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ina(CPU *cpu, memory_t *mem, unsigned short arg);
extern void inx(CPU *cpu, memory_t *mem, unsigned short arg);
extern void iny(CPU *cpu, memory_t *mem, unsigned short arg);
extern void dec_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void dea(CPU *cpu, memory_t *mem, unsigned short arg);
extern void dex(CPU *cpu, memory_t *mem, unsigned short arg);
extern void dey(CPU *cpu, memory_t *mem, unsigned short arg);
extern void asl_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void asla(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lsr_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void lsra(CPU *cpu, memory_t *mem, unsigned short arg);
extern void rol_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void rola(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ror_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void rora(CPU *cpu, memory_t *mem, unsigned short arg);
extern void and_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void and_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void eor_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void eor_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ora_imm(CPU *cpu, memory_t *mem, unsigned short arg);
extern void ora_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bit_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void jmp_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void jsr_abs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void rts(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bra(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bcc(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bcs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void beq(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bne(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bmi(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bpl(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bvs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void bvc(CPU *cpu, memory_t *mem, unsigned short arg);
extern void clc(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sec(CPU *cpu, memory_t *mem, unsigned short arg);
extern void cld(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sed(CPU *cpu, memory_t *mem, unsigned short arg);
extern void cli(CPU *cpu, memory_t *mem, unsigned short arg);
extern void sei(CPU *cpu, memory_t *mem, unsigned short arg);
extern void clv(CPU *cpu, memory_t *mem, unsigned short arg);
extern void tax(CPU *cpu, memory_t *mem, unsigned short arg);
extern void txa(CPU *cpu, memory_t *mem, unsigned short arg);
extern void tay(CPU *cpu, memory_t *mem, unsigned short arg);
extern void tya(CPU *cpu, memory_t *mem, unsigned short arg);
extern void tsx(CPU *cpu, memory_t *mem, unsigned short arg);
extern void txs(CPU *cpu, memory_t *mem, unsigned short arg);
extern void pha(CPU *cpu, memory_t *mem, unsigned short arg);
extern void pla(CPU *cpu, memory_t *mem, unsigned short arg);
extern void php(CPU *cpu, memory_t *mem, unsigned short arg);
extern void plp(CPU *cpu, memory_t *mem, unsigned short arg);
extern void op_brk(CPU *cpu, memory_t *mem, unsigned short arg);
extern void op_rti(CPU *cpu, memory_t *mem, unsigned short arg);
extern void op_nop(CPU *cpu, memory_t *mem, unsigned short arg);

opcode_handler_t opcodes_6502_undoc[] = {
	{"LDA", MODE_IMMEDIATE, lda_imm, 2, 0, 0, 0, {0xA9}, 1},
	{"LDA", MODE_ABSOLUTE, lda_abs, 4, 0, 0, 0, {0xAD}, 1},
	{"LDA", MODE_ABSOLUTE_X, lda_abs_x, 4, 0, 0, 0, {0xBD}, 1},
	{"LDA", MODE_ABSOLUTE_Y, lda_abs_y, 4, 0, 0, 0, {0xB9}, 1},
	{"LDA", MODE_ZP, lda_zp, 3, 0, 0, 0, {0xA5}, 1},
	{"LDA", MODE_ZP_X, lda_zp_x, 4, 0, 0, 0, {0xB5}, 1},
	{"LDA", MODE_INDIRECT_X, lda_ind_x, 6, 0, 0, 0, {0xA1}, 1},
	{"LDA", MODE_INDIRECT_Y, lda_ind_y, 5, 0, 0, 0, {0xB1}, 1},
	{"LDX", MODE_IMMEDIATE, ldx_imm, 2, 0, 0, 0, {0xA2}, 1},
	{"LDX", MODE_ABSOLUTE, ldx_abs, 4, 0, 0, 0, {0xAE}, 1},
	{"LDX", MODE_ABSOLUTE_Y, ldx_abs_y, 4, 0, 0, 0, {0xBE}, 1},
	{"LDX", MODE_ZP, ldx_zp, 3, 0, 0, 0, {0xA6}, 1},
	{"LDX", MODE_ZP_Y, ldx_zp_y, 4, 0, 0, 0, {0xB6}, 1},
	{"LDY", MODE_IMMEDIATE, ldy_imm, 2, 0, 0, 0, {0xA0}, 1},
	{"LDY", MODE_ABSOLUTE, ldy_abs, 4, 0, 0, 0, {0xAC}, 1},
	{"LDY", MODE_ABSOLUTE_X, ldy_abs_x, 4, 0, 0, 0, {0xBC}, 1},
	{"LDY", MODE_ZP, ldy_zp, 3, 0, 0, 0, {0xA4}, 1},
	{"LDY", MODE_ZP_X, ldy_zp_x, 4, 0, 0, 0, {0xB4}, 1},
	{"STA", MODE_ABSOLUTE, sta_abs, 4, 0, 0, 0, {0x8D}, 1},
	{"STA", MODE_ABSOLUTE_X, sta_abs_x, 5, 0, 0, 0, {0x9D}, 1},
	{"STA", MODE_ABSOLUTE_Y, sta_abs_y, 5, 0, 0, 0, {0x99}, 1},
	{"STA", MODE_ZP, sta_zp, 3, 0, 0, 0, {0x85}, 1},
	{"STA", MODE_ZP_X, sta_zp_x, 4, 0, 0, 0, {0x95}, 1},
	{"STA", MODE_INDIRECT_X, sta_ind_x, 6, 0, 0, 0, {0x81}, 1},
	{"STA", MODE_INDIRECT_Y, sta_ind_y, 6, 0, 0, 0, {0x91}, 1},
	{"STX", MODE_ABSOLUTE, stx_abs, 4, 0, 0, 0, {0x8E}, 1},
	{"STX", MODE_ZP, stx_zp, 3, 0, 0, 0, {0x86}, 1},
	{"STX", MODE_ZP_Y, stx_zp_y, 4, 0, 0, 0, {0x96}, 1},
	{"STY", MODE_ABSOLUTE, sty_abs, 4, 0, 0, 0, {0x8C}, 1},
	{"STY", MODE_ZP, sty_zp, 3, 0, 0, 0, {0x84}, 1},
	{"STY", MODE_ZP_X, sty_zp_x, 4, 0, 0, 0, {0x94}, 1},
	{"ADC", MODE_IMMEDIATE, adc_imm, 2, 0, 0, 0, {0x69}, 1},
	{"ADC", MODE_ABSOLUTE, adc_abs, 4, 0, 0, 0, {0x6D}, 1},
	{"ADC", MODE_ABSOLUTE_X, adc_abs_x, 4, 0, 0, 0, {0x7D}, 1},
	{"ADC", MODE_ABSOLUTE_Y, adc_abs_y, 4, 0, 0, 0, {0x79}, 1},
	{"ADC", MODE_ZP, adc_zp, 3, 0, 0, 0, {0x65}, 1},
	{"ADC", MODE_ZP_X, adc_zp_x, 4, 0, 0, 0, {0x75}, 1},
	{"ADC", MODE_INDIRECT_X, adc_ind_x, 6, 0, 0, 0, {0x61}, 1},
	{"ADC", MODE_INDIRECT_Y, adc_ind_y, 5, 0, 0, 0, {0x71}, 1},
	{"SBC", MODE_IMMEDIATE, sbc_imm, 2, 0, 0, 0, {0xE9}, 1},
	{"SBC", MODE_ABSOLUTE, sbc_abs, 4, 0, 0, 0, {0xED}, 1},
	{"CMP", MODE_IMMEDIATE, cmp_imm, 2, 0, 0, 0, {0xC9}, 1},
	{"CMP", MODE_ABSOLUTE, cmp_abs, 4, 0, 0, 0, {0xCD}, 1},
	{"CPX", MODE_IMMEDIATE, cpx_imm, 2, 0, 0, 0, {0xE0}, 1},
	{"CPY", MODE_IMMEDIATE, cpy_imm, 2, 0, 0, 0, {0xC0}, 1},
	{"INC", MODE_ABSOLUTE, inc_abs, 6, 0, 0, 0, {0xEE}, 1},
	{"INA", MODE_IMPLIED, ina, 2, 0, 0, 0, {0x1A}, 1},
	{"INX", MODE_IMPLIED, inx, 2, 0, 0, 0, {0xE8}, 1},
	{"INY", MODE_IMPLIED, iny, 2, 0, 0, 0, {0xC8}, 1},
	{"DEC", MODE_ABSOLUTE, dec_abs, 6, 0, 0, 0, {0xCE}, 1},
	{"DEA", MODE_IMPLIED, dea, 2, 0, 0, 0, {0x3A}, 1},
	{"DEX", MODE_IMPLIED, dex, 2, 0, 0, 0, {0xCA}, 1},
	{"DEY", MODE_IMPLIED, dey, 2, 0, 0, 0, {0x88}, 1},
	{"ASL", MODE_ABSOLUTE, asl_abs, 6, 0, 0, 0, {0x0E}, 1},
	{"ASL", MODE_IMPLIED, asla, 2, 0, 0, 0, {0x0A}, 1},
	{"LSR", MODE_ABSOLUTE, lsr_abs, 6, 0, 0, 0, {0x4E}, 1},
	{"LSR", MODE_IMPLIED, lsra, 2, 0, 0, 0, {0x4A}, 1},
	{"ROL", MODE_ABSOLUTE, rol_abs, 6, 0, 0, 0, {0x2E}, 1},
	{"ROL", MODE_IMPLIED, rola, 2, 0, 0, 0, {0x2A}, 1},
	{"ROR", MODE_ABSOLUTE, ror_abs, 6, 0, 0, 0, {0x6E}, 1},
	{"ROR", MODE_IMPLIED, rora, 2, 0, 0, 0, {0x6A}, 1},
	{"AND", MODE_IMMEDIATE, and_imm, 2, 0, 0, 0, {0x29}, 1},
	{"AND", MODE_ABSOLUTE, and_abs, 4, 0, 0, 0, {0x2D}, 1},
	{"EOR", MODE_IMMEDIATE, eor_imm, 2, 0, 0, 0, {0x49}, 1},
	{"EOR", MODE_ABSOLUTE, eor_abs, 4, 0, 0, 0, {0x4D}, 1},
	{"ORA", MODE_IMMEDIATE, ora_imm, 2, 0, 0, 0, {0x09}, 1},
	{"ORA", MODE_ABSOLUTE, ora_abs, 4, 0, 0, 0, {0x0D}, 1},
	{"BIT", MODE_ABSOLUTE, bit_abs, 4, 0, 0, 0, {0x2C}, 1},
	{"JMP", MODE_ABSOLUTE, jmp_abs, 3, 0, 0, 0, {0x4C}, 1},
	{"JSR", MODE_ABSOLUTE, jsr_abs, 6, 0, 0, 0, {0x20}, 1},
	{"RTS", MODE_IMPLIED, rts, 6, 0, 0, 0, {0x60}, 1},
	{"BRA", MODE_RELATIVE, bra, 2, 0, 0, 0, {0x80}, 1},
	{"BCC", MODE_RELATIVE, bcc, 2, 0, 0, 0, {0x90}, 1},
	{"BCS", MODE_RELATIVE, bcs, 2, 0, 0, 0, {0xB0}, 1},
	{"BEQ", MODE_RELATIVE, beq, 2, 0, 0, 0, {0xF0}, 1},
	{"BNE", MODE_RELATIVE, bne, 2, 0, 0, 0, {0xD0}, 1},
	{"BMI", MODE_RELATIVE, bmi, 2, 0, 0, 0, {0x30}, 1},
	{"BPL", MODE_RELATIVE, bpl, 2, 0, 0, 0, {0x10}, 1},
	{"BVS", MODE_RELATIVE, bvs, 2, 0, 0, 0, {0x70}, 1},
	{"BVC", MODE_RELATIVE, bvc, 2, 0, 0, 0, {0x50}, 1},
	{"CLC", MODE_IMPLIED, clc, 2, 0, 0, 0, {0x18}, 1},
	{"SEC", MODE_IMPLIED, sec, 2, 0, 0, 0, {0x38}, 1},
	{"CLD", MODE_IMPLIED, cld, 2, 0, 0, 0, {0xD8}, 1},
	{"SED", MODE_IMPLIED, sed, 2, 0, 0, 0, {0xF8}, 1},
	{"CLI", MODE_IMPLIED, cli, 2, 0, 0, 0, {0x58}, 1},
	{"SEI", MODE_IMPLIED, sei, 2, 0, 0, 0, {0x78}, 1},
	{"CLV", MODE_IMPLIED, clv, 2, 0, 0, 0, {0xB8}, 1},
	{"TAX", MODE_IMPLIED, tax, 2, 0, 0, 0, {0xAA}, 1},
	{"TXA", MODE_IMPLIED, txa, 2, 0, 0, 0, {0x8A}, 1},
	{"TAY", MODE_IMPLIED, tay, 2, 0, 0, 0, {0xA8}, 1},
	{"TYA", MODE_IMPLIED, tya, 2, 0, 0, 0, {0x98}, 1},
	{"TSX", MODE_IMPLIED, tsx, 2, 0, 0, 0, {0xBA}, 1},
	{"TXS", MODE_IMPLIED, txs, 2, 0, 0, 0, {0x9A}, 1},
	{"PHA", MODE_IMPLIED, pha, 3, 0, 0, 0, {0x48}, 1},
	{"PLA", MODE_IMPLIED, pla, 4, 0, 0, 0, {0x68}, 1},
	{"PHP", MODE_IMPLIED, php, 3, 0, 0, 0, {0x08}, 1},
	{"PLP", MODE_IMPLIED, plp, 4, 0, 0, 0, {0x28}, 1},
	{"BRK", MODE_IMPLIED, op_brk, 7, 0, 0, 0, {0x00}, 1},
	{"RTI", MODE_IMPLIED, op_rti, 6, 0, 0, 0, {0x40}, 1},
	{"NOP", MODE_IMPLIED, op_nop, 2, 0, 0, 0, {0xEA}, 1},
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
	{"ASR", MODE_IMMEDIATE, asr_imm, 2, 0, 0, 0, {0x4B}, 1},
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
