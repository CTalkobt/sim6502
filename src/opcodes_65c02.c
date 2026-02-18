#include "opcodes.h"

void bra_rel(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	signed char offset = arg & 0xFF;
	cpu->pc += 2 + offset;
}

void bit_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void bit_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	set_flag(cpu, FLAG_N, val & 0x80);
	set_flag(cpu, FLAG_V, val & 0x40);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void bit_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	set_flag(cpu, FLAG_N, val & 0x80);
	set_flag(cpu, FLAG_V, val & 0x40);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void trb_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	val &= ~cpu->a;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void trb_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	val &= ~cpu->a;
	mem_write(mem, arg, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void tsb_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	val |= cpu->a;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void tsb_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	val |= cpu->a;
	mem_write(mem, arg, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void rmb0(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xFE;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb1(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xFD;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb2(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xFB;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb3(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xF7;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb4(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xEF;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb5(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xDF;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb6(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0xBF;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rmb7(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val &= 0x7F;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb0(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x01;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb1(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x02;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb2(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x04;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb3(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x08;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb4(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x10;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb5(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x20;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb6(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x40;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void smb7(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	val |= 0x80;
	mem_write(mem, arg & 0xFF, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void bbr0(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x01) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr1(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x02) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr2(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x04) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr3(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x08) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr4(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x10) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr5(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x20) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr6(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x40) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbr7(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x80) == 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs0(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x01) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs1(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x02) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs2(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x04) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs3(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x08) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs4(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x10) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs5(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x20) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs6(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x40) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void bbs7(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = arg & 0xFF;
	unsigned char val = mem_read(mem, zp);
	if ((val & 0x80) != 0) {
		signed char offset = (arg >> 8) & 0xFF;
		cpu->pc += 3 + offset;
	} else {
		cpu->pc += 3;
	}
}

void stz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, 0);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void stz_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->x, 0);
	cpu->cycles += 5;
	cpu->pc += 3;
}

void stz_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, 0);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void stz_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->x) & 0xFF, 0);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void brl(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	signed short offset = (signed short)arg;
	cpu->pc += 3 + offset;
}

void jmp_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->pc = addr;
}

extern void lda_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lda_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldx_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldx_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldx_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldx_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldx_zp_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldy_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldy_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldy_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldy_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ldy_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sta_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stx_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stx_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stx_zp_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sty_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sty_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sty_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sbc_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sbc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cmp_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cmp_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cpx_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cpy_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void inc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ina(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void inx(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void iny(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void dec_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void dea(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void dex(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void dey(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void asl_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void asla(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lsr_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void lsra(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rol_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rola(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ror_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rora(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void and_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void and_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void eor_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void eor_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ora_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ora_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bit_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void jmp_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void jsr_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rts(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bcc(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bcs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void beq(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bne(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bmi(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bpl(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bvs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bvc(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void clc(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sec(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cld(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sed(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cli(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sei(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void clv(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void tax(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void txa(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void tay(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void tya(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void tsx(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void txs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void pha(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void pla(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void php(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void plp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void brk(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rti(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void nop(cpu_t *cpu, memory_t *mem, unsigned short arg);

opcode_handler_t opcodes_65c02[] = {
	{"LDA", MODE_IMMEDIATE, lda_imm, 2},
	{"LDA", MODE_ABSOLUTE, lda_abs, 4},
	{"LDA", MODE_ABSOLUTE_X, lda_abs_x, 4, 0, 0, 0, 0xBD},
	{"LDA", MODE_ABSOLUTE_Y, lda_abs_y, 4, 0, 0, 0, 0xB9},
	{"LDA", MODE_ZP, lda_zp, 3},
	{"LDA", MODE_ZP_X, lda_zp_x, 4, 0, 0, 0, 0xB5},
	{"LDA", MODE_INDIRECT_X, lda_ind_x, 6},
	{"LDA", MODE_INDIRECT_Y, lda_ind_y, 5, 0, 0, 0, 0xB1},
	{"LDX", MODE_IMMEDIATE, ldx_imm, 2},
	{"LDX", MODE_ABSOLUTE, ldx_abs, 4},
	{"LDX", MODE_ABSOLUTE_Y, ldx_abs_y, 4, 0, 0, 0, 0xBE},
	{"LDX", MODE_ZP, ldx_zp, 3},
	{"LDX", MODE_ZP_Y, ldx_zp_y, 4, 0, 0, 0, 0xB6},
	{"LDY", MODE_IMMEDIATE, ldy_imm, 2},
	{"LDY", MODE_ABSOLUTE, ldy_abs, 4},
	{"LDY", MODE_ABSOLUTE_X, ldy_abs_x, 4, 0, 0, 0, 0xBC},
	{"LDY", MODE_ZP, ldy_zp, 3},
	{"LDY", MODE_ZP_X, ldy_zp_x, 4, 0, 0, 0, 0xB4},
	{"STA", MODE_ABSOLUTE, sta_abs, 4, 0, 0, 0, 0x8D},
	{"STA", MODE_ABSOLUTE_X, sta_abs_x, 5, 0, 0, 0, 0x9D},
	{"STA", MODE_ABSOLUTE_Y, sta_abs_y, 5, 0, 0, 0, 0x99},
	{"STA", MODE_ZP, sta_zp, 3, 0, 0, 0, 0x85},
	{"STA", MODE_ZP_X, sta_zp_x, 4, 0, 0, 0, 0x95},
	{"STA", MODE_INDIRECT_X, sta_ind_x, 6, 0, 0, 0, 0x81},
	{"STA", MODE_INDIRECT_Y, sta_ind_y, 6, 0, 0, 0, 0x91},
	{"STX", MODE_ABSOLUTE, stx_abs, 4, 0, 0, 0, 0x8E},
	{"STX", MODE_ZP, stx_zp, 3, 0, 0, 0, 0x86},
	{"STX", MODE_ZP_Y, stx_zp_y, 4, 0, 0, 0, 0x96},
	{"STY", MODE_ABSOLUTE, sty_abs, 4, 0, 0, 0, 0x8C},
	{"STY", MODE_ZP, sty_zp, 3, 0, 0, 0, 0x84},
	{"STY", MODE_ZP_X, sty_zp_x, 4, 0, 0, 0, 0x94},
	{"ADC", MODE_IMMEDIATE, adc_imm, 2, 0, 0, 0, 0x69},
	{"ADC", MODE_ABSOLUTE, adc_abs, 4, 0, 0, 0, 0x6D},
	{"ADC", MODE_ABSOLUTE_X, adc_abs_x, 4, 0, 0, 0, 0x7D},
	{"ADC", MODE_ABSOLUTE_Y, adc_abs_y, 4, 0, 0, 0, 0x79},
	{"ADC", MODE_ZP, adc_zp, 3, 0, 0, 0, 0x65},
	{"ADC", MODE_ZP_X, adc_zp_x, 4, 0, 0, 0, 0x75},
	{"ADC", MODE_INDIRECT_X, adc_ind_x, 6, 0, 0, 0, 0x61},
	{"ADC", MODE_INDIRECT_Y, adc_ind_y, 5, 0, 0, 0, 0x71},
	{"SBC", MODE_IMMEDIATE, sbc_imm, 2},
	{"SBC", MODE_ABSOLUTE, sbc_abs, 4},
	{"CMP", MODE_IMMEDIATE, cmp_imm, 2},
	{"CMP", MODE_ABSOLUTE, cmp_abs, 4},
	{"CPX", MODE_IMMEDIATE, cpx_imm, 2},
	{"CPY", MODE_IMMEDIATE, cpy_imm, 2},
	{"INC", MODE_ABSOLUTE, inc_abs, 6},
	{"INA", MODE_IMPLIED, ina, 2},
	{"INX", MODE_IMPLIED, inx, 2},
	{"INY", MODE_IMPLIED, iny, 2},
	{"DEC", MODE_ABSOLUTE, dec_abs, 6},
	{"DEA", MODE_IMPLIED, dea, 2},
	{"DEX", MODE_IMPLIED, dex, 2},
	{"DEY", MODE_IMPLIED, dey, 2, 0, 0, 0, 0x88},
	{"ASL", MODE_ABSOLUTE, asl_abs, 6, 0, 0, 0, 0x0E},
	{"ASL", MODE_IMPLIED, asla, 2, 0, 0, 0, 0x0A},
	{"LSR", MODE_ABSOLUTE, lsr_abs, 6, 0, 0, 0, 0x4E},
	{"LSR", MODE_IMPLIED, lsra, 2, 0, 0, 0, 0x4A},
	{"ROL", MODE_ABSOLUTE, rol_abs, 6, 0, 0, 0, 0x2E},
	{"ROL", MODE_IMPLIED, rola, 2, 0, 0, 0, 0x2A},
	{"ROR", MODE_ABSOLUTE, ror_abs, 6, 0, 0, 0, 0x6E},
	{"ROR", MODE_IMPLIED, rora, 2, 0, 0, 0, 0x6A},
	{"AND", MODE_IMMEDIATE, and_imm, 2, 0, 0, 0, 0x29},
	{"AND", MODE_ABSOLUTE, and_abs, 4, 0, 0, 0, 0x2D},
	{"EOR", MODE_IMMEDIATE, eor_imm, 2, 0, 0, 0, 0x49},
	{"EOR", MODE_ABSOLUTE, eor_abs, 4, 0, 0, 0, 0x4D},
	{"ORA", MODE_IMMEDIATE, ora_imm, 2, 0, 0, 0, 0x09},
	{"ORA", MODE_ABSOLUTE, ora_abs, 4, 0, 0, 0, 0x0D},
	{"BIT", MODE_ABSOLUTE, bit_abs, 4, 0, 0, 0, 0x2C},
	{"BIT", MODE_IMMEDIATE, bit_imm, 2, 0, 0, 0, 0x89},
	{"BIT", MODE_ZP_X, bit_zp_x, 4, 0, 0, 0, 0x34},
	{"BIT", MODE_ABSOLUTE_X, bit_abs_x, 4, 0, 0, 0, 0x3C},
	{"TRB", MODE_ZP, trb_zp, 5, 0, 0, 0, 0x14},
	{"TRB", MODE_ABSOLUTE, trb_abs, 6, 0, 0, 0, 0x1C},
	{"TSB", MODE_ZP, tsb_zp, 5, 0, 0, 0, 0x04},
	{"TSB", MODE_ABSOLUTE, tsb_abs, 6, 0, 0, 0, 0x0C},
	{"RMB0", MODE_ZP, rmb0, 5, 0, 0, 0, 0x07},
	{"RMB1", MODE_ZP, rmb1, 5, 0, 0, 0, 0x17},
	{"RMB2", MODE_ZP, rmb2, 5, 0, 0, 0, 0x27},
	{"RMB3", MODE_ZP, rmb3, 5, 0, 0, 0, 0x37},
	{"RMB4", MODE_ZP, rmb4, 5, 0, 0, 0, 0x47},
	{"RMB5", MODE_ZP, rmb5, 5, 0, 0, 0, 0x57},
	{"RMB6", MODE_ZP, rmb6, 5, 0, 0, 0, 0x67},
	{"RMB7", MODE_ZP, rmb7, 5, 0, 0, 0, 0x77},
	{"SMB0", MODE_ZP, smb0, 5, 0, 0, 0, 0x87},
	{"SMB1", MODE_ZP, smb1, 5, 0, 0, 0, 0x97},
	{"SMB2", MODE_ZP, smb2, 5},
	{"SMB3", MODE_ZP, smb3, 5, 0, 0, 0, 0xB7},
	{"SMB4", MODE_ZP, smb4, 5},
	{"SMB5", MODE_ZP, smb5, 5, 0, 0, 0, 0xD7},
	{"SMB6", MODE_ZP, smb6, 5},
	{"SMB7", MODE_ZP, smb7, 5, 0, 0, 0, 0xF7},
	{"BBR0", MODE_ZP, bbr0, 5, 0, 0, 0, 0x0F},
	{"BBR1", MODE_ZP, bbr1, 5, 0, 0, 0, 0x1F},
	{"BBR2", MODE_ZP, bbr2, 5, 0, 0, 0, 0x2F},
	{"BBR3", MODE_ZP, bbr3, 5, 0, 0, 0, 0x3F},
	{"BBR4", MODE_ZP, bbr4, 5, 0, 0, 0, 0x4F},
	{"BBR5", MODE_ZP, bbr5, 5, 0, 0, 0, 0x5F},
	{"BBR6", MODE_ZP, bbr6, 5, 0, 0, 0, 0x6F},
	{"BBR7", MODE_ZP, bbr7, 5, 0, 0, 0, 0x7F},
	{"BBS0", MODE_ZP, bbs0, 5, 0, 0, 0, 0x8F},
	{"BBS1", MODE_ZP, bbs1, 5, 0, 0, 0, 0x9F},
	{"BBS2", MODE_ZP, bbs2, 5},
	{"BBS3", MODE_ZP, bbs3, 5, 0, 0, 0, 0xBF},
	{"BBS4", MODE_ZP, bbs4, 5},
	{"BBS5", MODE_ZP, bbs5, 5, 0, 0, 0, 0xDF},
	{"BBS6", MODE_ZP, bbs6, 5},
	{"BBS7", MODE_ZP, bbs7, 5, 0, 0, 0, 0xFF},
	{"STZ", MODE_ABSOLUTE, stz_abs, 4, 0, 0, 0, 0x9C},
	{"STZ", MODE_ABSOLUTE_X, stz_abs_x, 5, 0, 0, 0, 0x9E},
	{"STZ", MODE_ZP, stz_zp, 3, 0, 0, 0, 0x64},
	{"STZ", MODE_ZP_X, stz_zp_x, 4, 0, 0, 0, 0x74},
	{"BRA", MODE_RELATIVE, bra_rel, 3, 0, 0, 0, 0x80},
	{"BRL", MODE_RELATIVE, brl, 4},
	{"JMP", MODE_ABSOLUTE, jmp_abs, 3, 0, 0, 0, 0x4C},
	{"JMP", MODE_INDIRECT_X, jmp_ind_x, 6, 0, 0, 0, 0x7C},
	{"JSR", MODE_ABSOLUTE, jsr_abs, 6, 0, 0, 0, 0x20},
	{"RTS", MODE_IMPLIED, rts, 6, 0, 0, 0, 0x60},
	{"BCC", MODE_RELATIVE, bcc, 2, 0, 0, 0, 0x90},
	{"BCS", MODE_RELATIVE, bcs, 2, 0, 0, 0, 0xB0},
	{"BEQ", MODE_RELATIVE, beq, 2, 0, 0, 0, 0xF0},
	{"BNE", MODE_RELATIVE, bne, 2, 0, 0, 0, 0xD0},
	{"BMI", MODE_RELATIVE, bmi, 2, 0, 0, 0, 0x30},
	{"BPL", MODE_RELATIVE, bpl, 2, 0, 0, 0, 0x10},
	{"BVS", MODE_RELATIVE, bvs, 2, 0, 0, 0, 0x70},
	{"BVC", MODE_RELATIVE, bvc, 2, 0, 0, 0, 0x50},
	{"CLC", MODE_IMPLIED, clc, 2, 0, 0, 0, 0x18},
	{"SEC", MODE_IMPLIED, sec, 2, 0, 0, 0, 0x38},
	{"CLD", MODE_IMPLIED, cld, 2, 0, 0, 0, 0xD8},
	{"SED", MODE_IMPLIED, sed, 2, 0, 0, 0, 0xF8},
	{"CLI", MODE_IMPLIED, cli, 2, 0, 0, 0, 0x58},
	{"SEI", MODE_IMPLIED, sei, 2, 0, 0, 0, 0x78},
	{"CLV", MODE_IMPLIED, clv, 2, 0, 0, 0, 0xB8},
	{"TAX", MODE_IMPLIED, tax, 2},
	{"TXA", MODE_IMPLIED, txa, 2, 0, 0, 0, 0x8A},
	{"TAY", MODE_IMPLIED, tay, 2},
	{"TYA", MODE_IMPLIED, tya, 2, 0, 0, 0, 0x98},
	{"TSX", MODE_IMPLIED, tsx, 2, 0, 0, 0, 0xBA},
	{"TXS", MODE_IMPLIED, txs, 2, 0, 0, 0, 0x9A},
	{"PHA", MODE_IMPLIED, pha, 3, 0, 0, 0, 0x48},
	{"PLA", MODE_IMPLIED, pla, 4, 0, 0, 0, 0x68},
	{"PHP", MODE_IMPLIED, php, 3, 0, 0, 0, 0x08},
	{"PLP", MODE_IMPLIED, plp, 4, 0, 0, 0, 0x28},
	{"BRK", MODE_IMPLIED, brk, 7, 0, 0, 0, 0x00},
	{"RTI", MODE_IMPLIED, rti, 6, 0, 0, 0, 0x40},
	{"NOP", MODE_IMPLIED, nop, 2},
};

int OPCODES_65C02_COUNT = sizeof(opcodes_65c02) / sizeof(opcodes_65c02[0]);
