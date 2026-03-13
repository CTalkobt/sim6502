#include "opcodes.h"

void lda_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = arg & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void lda_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void lda_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void lda_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void lda_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void lda_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void lda_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a = mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void lda_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a = mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void ldx_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = arg & 0xFF;
	cpu->update_nz(cpu->x);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void ldx_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, arg);
	cpu->update_nz(cpu->x);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldx_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->x);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldx_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->x);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void ldx_zp_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, (arg + cpu->y) & 0xFF);
	cpu->update_nz(cpu->x);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void ldy_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = arg & 0xFF;
	cpu->update_nz(cpu->y);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void ldy_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, arg);
	cpu->update_nz(cpu->y);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldy_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->y);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldy_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->y);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void ldy_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->y);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sta_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void sta_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->x, cpu->a);
	cpu->cycles += 5;
	cpu->pc += 3;
}

void sta_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->y, cpu->a);
	cpu->cycles += 5;
	cpu->pc += 3;
}

void sta_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void sta_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->x) & 0xFF, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sta_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	mem_write(mem, addr, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void sta_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	mem_write(mem, addr + cpu->y, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void stx_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void stx_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->x);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void stx_zp_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->y) & 0xFF, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sty_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void sty_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->y);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void sty_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->x) & 0xFF, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void adc_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	cpu->do_adc(val);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void adc_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->do_adc(val);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void adc_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	cpu->do_adc(val);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void adc_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->y);
	cpu->do_adc(val);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void adc_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->do_adc(val);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void adc_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->do_adc(val);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void adc_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr);
	cpu->do_adc(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void adc_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr + cpu->y);
	cpu->do_adc(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void sbc_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	cpu->do_sbc(val);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void sbc_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->do_sbc(val);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void cmp_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void cmp_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void cpx_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->x - val;
	cpu->set_flag(FLAG_C, cpu->x >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void cpy_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->y - val;
	cpu->set_flag(FLAG_C, cpu->y >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void inc_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg) + 1) & 0xFF;
	mem_write(mem, arg, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void ina(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = (cpu->a + 1) & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void inx(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = (cpu->x + 1) & 0xFF;
	cpu->update_nz(cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void iny(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = (cpu->y + 1) & 0xFF;
	cpu->update_nz(cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void dec_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg) - 1) & 0xFF;
	mem_write(mem, arg, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void dea(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = (cpu->a - 1) & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void dex(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = (cpu->x - 1) & 0xFF;
	cpu->update_nz(cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void dey(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = (cpu->y - 1) & 0xFF;
	cpu->update_nz(cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void asl_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void asla(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_C, cpu->a & 0x80);
	cpu->a = (cpu->a << 1) & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void lsr_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void lsra(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a >>= 1;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void rol_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void rola(CPU *cpu, memory_t *mem, unsigned short arg) {
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, cpu->a & 0x80);
	cpu->a = ((cpu->a << 1) | c) & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void ror_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void rora(CPU *cpu, memory_t *mem, unsigned short arg) {
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a = ((cpu->a >> 1) | (c << 7)) & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void and_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= arg & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void and_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void eor_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= arg & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void eor_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ora_imm(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= arg & 0xFF;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void ora_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void bit_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_Z, (cpu->a & val) == 0);
	cpu->set_flag(FLAG_N, val & 0x80);
	cpu->set_flag(FLAG_V, val & 0x40);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void bit_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_Z, (cpu->a & val) == 0);
	cpu->set_flag(FLAG_N, val & 0x80);
	cpu->set_flag(FLAG_V, val & 0x40);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void jmp_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 3;
	cpu->pc = arg;
}

void jsr_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, 0x100 + cpu->s, (ret >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, ret & 0xFF);
	cpu->s--;
	cpu->cycles += 6;
	cpu->pc = arg;
}

void rts(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	unsigned short ret = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	ret |= mem_read(mem, 0x100 + cpu->s) << 8;
	cpu->cycles += 6;
	cpu->pc = ret + 1;
}

void bra(CPU *cpu, memory_t *mem, unsigned short arg) {
	signed char offset = arg & 0xFF;
	cpu->cycles += 3;
	cpu->pc += 2 + offset;
}

void bcc(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_C))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bcs(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_C))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void beq(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_Z))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bne(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_Z))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bmi(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_N))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bpl(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_N))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bvs(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_V))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bvc(CPU *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_V))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void clc(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_C, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void sec(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_C, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void cld(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_D, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void sed(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_D, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void cli(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_I, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void sei(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_I, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void clv(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_V, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tax(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = cpu->a;
	cpu->update_nz(cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void txa(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->x;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tay(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = cpu->a;
	cpu->update_nz(cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tya(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->y;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tsx(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = cpu->s;
	cpu->update_nz(cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void txs(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->s = cpu->x;
	cpu->cycles += 2;
	cpu->pc += 1;
}

void pha(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, cpu->a);
	cpu->s--;
	cpu->cycles += 3;
	cpu->pc += 1;
}

void pla(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->a = mem_read(mem, 0x100 + cpu->s);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 1;
}

void php(CPU *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, cpu->p | FLAG_B | FLAG_U);
	cpu->s--;
	cpu->cycles += 3;
	cpu->pc += 1;
}

void plp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->p = mem_read(mem, 0x100 + cpu->s);
	cpu->cycles += 4;
	cpu->pc += 1;
}

void op_brk(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 7;
	cpu->pc += 2;
	mem_write(mem, 0x100 + cpu->s, (cpu->pc >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, cpu->pc & 0xFF);
	cpu->s--;
	cpu->set_flag(FLAG_B, 1);
	mem_write(mem, 0x100 + cpu->s, cpu->p);
	cpu->s--;
	cpu->set_flag(FLAG_I, 1);
}

void op_rti(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->p = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	unsigned short ret = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	ret |= mem_read(mem, 0x100 + cpu->s) << 8;
	cpu->pc = ret;
}

void op_nop(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 2;
	cpu->pc += 1;
}

/* ---- INC missing modes ---- */
void inc_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) + 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void inc_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (arg + cpu->x) & 0xFF;
	unsigned char val = (mem_read(mem, addr) + 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void inc_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = arg + cpu->x;
	unsigned char val = (mem_read(mem, addr) + 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* ---- DEC missing modes ---- */
void dec_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) - 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void dec_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (arg + cpu->x) & 0xFF;
	unsigned char val = (mem_read(mem, addr) - 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void dec_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = arg + cpu->x;
	unsigned char val = (mem_read(mem, addr) - 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* ---- AND missing modes ---- */
void and_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void and_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void and_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void and_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void and_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a &= mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void and_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a &= mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

/* ---- ORA missing modes ---- */
void ora_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void ora_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void ora_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ora_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ora_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a |= mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void ora_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a |= mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

/* ---- EOR missing modes ---- */
void eor_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void eor_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void eor_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void eor_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void eor_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a ^= mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void eor_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a ^= mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

/* ---- CMP missing modes ---- */
void cmp_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void cmp_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void cmp_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void cmp_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->y);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void cmp_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void cmp_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr + cpu->y);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 5;
	cpu->pc += 2;
}

/* ---- SBC missing modes ---- */
void sbc_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->do_sbc(mem_read(mem, arg & 0xFF));
	cpu->cycles += 3;
	cpu->pc += 2;
}

void sbc_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->do_sbc(mem_read(mem, (arg + cpu->x) & 0xFF));
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sbc_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->do_sbc(mem_read(mem, arg + cpu->x));
	cpu->cycles += 4;
	cpu->pc += 3;
}

void sbc_abs_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	cpu->do_sbc(mem_read(mem, arg + cpu->y));
	cpu->cycles += 4;
	cpu->pc += 3;
}

void sbc_ind_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->do_sbc(mem_read(mem, addr));
	cpu->cycles += 6;
	cpu->pc += 2;
}

void sbc_ind_y(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->do_sbc(mem_read(mem, addr + cpu->y));
	cpu->cycles += 5;
	cpu->pc += 2;
}

/* ---- ASL missing modes ---- */
void asl_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void asl_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (arg + cpu->x) & 0xFF;
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void asl_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = arg + cpu->x;
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* ---- LSR missing modes ---- */
void lsr_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void lsr_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (arg + cpu->x) & 0xFF;
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void lsr_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = arg + cpu->x;
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* ---- ROL missing modes ---- */
void rol_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void rol_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (arg + cpu->x) & 0xFF;
	unsigned char val = mem_read(mem, addr);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void rol_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = arg + cpu->x;
	unsigned char val = mem_read(mem, addr);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* ---- ROR missing modes ---- */
void ror_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void ror_zp_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (arg + cpu->x) & 0xFF;
	unsigned char val = mem_read(mem, addr);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void ror_abs_x(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = arg + cpu->x;
	unsigned char val = mem_read(mem, addr);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, addr, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* ---- CPX missing modes ---- */
void cpx_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, cpu->x >= val);
	cpu->update_nz((cpu->x - val) & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void cpx_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, cpu->x >= val);
	cpu->update_nz((cpu->x - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

/* ---- CPY missing modes ---- */
void cpy_zp(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, cpu->y >= val);
	cpu->update_nz((cpu->y - val) & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void cpy_abs(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, cpu->y >= val);
	cpu->update_nz((cpu->y - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

/* ---- JMP indirect (with NMOS page-wrap bug) ---- */
void jmp_ind(CPU *cpu, memory_t *mem, unsigned short arg) {
	unsigned short lo = mem_read(mem, arg);
	unsigned short hi = mem_read(mem, (arg & 0xFF00) | ((arg + 1) & 0x00FF));
	cpu->cycles += 5;
	cpu->pc = lo | (hi << 8);
}

opcode_handler_t opcodes_6502[] = {
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
	{"SBC", MODE_IMMEDIATE,  sbc_imm,   2, 0, 0, 0, {0xE9}, 1},
	{"SBC", MODE_ABSOLUTE,   sbc_abs,   4, 0, 0, 0, {0xED}, 1},
	{"SBC", MODE_ZP,        sbc_zp,    3, 0, 0, 0, {0xE5}, 1},
	{"SBC", MODE_ZP_X,     sbc_zp_x,  4, 0, 0, 0, {0xF5}, 1},
	{"SBC", MODE_ABSOLUTE_X, sbc_abs_x, 4, 0, 0, 0, {0xFD}, 1},
	{"SBC", MODE_ABSOLUTE_Y, sbc_abs_y, 4, 0, 0, 0, {0xF9}, 1},
	{"SBC", MODE_INDIRECT_X, sbc_ind_x, 6, 0, 0, 0, {0xE1}, 1},
	{"SBC", MODE_INDIRECT_Y, sbc_ind_y, 5, 0, 0, 0, {0xF1}, 1},
	{"CMP", MODE_IMMEDIATE,  cmp_imm,   2, 0, 0, 0, {0xC9}, 1},
	{"CMP", MODE_ABSOLUTE,   cmp_abs,   4, 0, 0, 0, {0xCD}, 1},
	{"CMP", MODE_ZP,        cmp_zp,    3, 0, 0, 0, {0xC5}, 1},
	{"CMP", MODE_ZP_X,     cmp_zp_x,  4, 0, 0, 0, {0xD5}, 1},
	{"CMP", MODE_ABSOLUTE_X, cmp_abs_x, 4, 0, 0, 0, {0xDD}, 1},
	{"CMP", MODE_ABSOLUTE_Y, cmp_abs_y, 4, 0, 0, 0, {0xD9}, 1},
	{"CMP", MODE_INDIRECT_X, cmp_ind_x, 6, 0, 0, 0, {0xC1}, 1},
	{"CMP", MODE_INDIRECT_Y, cmp_ind_y, 5, 0, 0, 0, {0xD1}, 1},
	{"CPX", MODE_IMMEDIATE,  cpx_imm,   2, 0, 0, 0, {0xE0}, 1},
	{"CPX", MODE_ZP,        cpx_zp,    3, 0, 0, 0, {0xE4}, 1},
	{"CPX", MODE_ABSOLUTE,   cpx_abs,   4, 0, 0, 0, {0xEC}, 1},
	{"CPY", MODE_IMMEDIATE,  cpy_imm,   2, 0, 0, 0, {0xC0}, 1},
	{"CPY", MODE_ZP,        cpy_zp,    3, 0, 0, 0, {0xC4}, 1},
	{"CPY", MODE_ABSOLUTE,   cpy_abs,   4, 0, 0, 0, {0xCC}, 1},
	{"INC", MODE_ABSOLUTE, inc_abs, 6, 0, 0, 0, {0xEE}, 1},
	{"INC", MODE_ZP,       inc_zp,     5, 0, 0, 0, {0xE6}, 1},
	{"INC", MODE_ZP_X,    inc_zp_x,   6, 0, 0, 0, {0xF6}, 1},
	{"INC", MODE_ABSOLUTE_X, inc_abs_x, 7, 0, 0, 0, {0xFE}, 1},
	{"INA", MODE_IMPLIED, ina, 2, 0, 0, 0, {0x1A}, 1},
	{"INX", MODE_IMPLIED, inx, 2, 0, 0, 0, {0xE8}, 1},
	{"INY", MODE_IMPLIED, iny, 2, 0, 0, 0, {0xC8}, 1},
	{"DEC", MODE_ABSOLUTE, dec_abs, 6, 0, 0, 0, {0xCE}, 1},
	{"DEC", MODE_ZP,       dec_zp,     5, 0, 0, 0, {0xC6}, 1},
	{"DEC", MODE_ZP_X,    dec_zp_x,   6, 0, 0, 0, {0xD6}, 1},
	{"DEC", MODE_ABSOLUTE_X, dec_abs_x, 7, 0, 0, 0, {0xDE}, 1},
	{"DEA", MODE_IMPLIED, dea, 2, 0, 0, 0, {0x3A}, 1},
	{"DEX", MODE_IMPLIED, dex, 2, 0, 0, 0, {0xCA}, 1},
	{"DEY", MODE_IMPLIED, dey, 2, 0, 0, 0, {0x88}, 1},
	{"ASL", MODE_ABSOLUTE,   asl_abs,   6, 0, 0, 0, {0x0E}, 1},
	{"ASL", MODE_ZP,        asl_zp,    5, 0, 0, 0, {0x06}, 1},
	{"ASL", MODE_ZP_X,     asl_zp_x,  6, 0, 0, 0, {0x16}, 1},
	{"ASL", MODE_ABSOLUTE_X, asl_abs_x, 7, 0, 0, 0, {0x1E}, 1},
	{"ASL", MODE_IMPLIED,   asla,      2, 0, 0, 0, {0x0A}, 1},
	{"LSR", MODE_ABSOLUTE,   lsr_abs,   6, 0, 0, 0, {0x4E}, 1},
	{"LSR", MODE_ZP,        lsr_zp,    5, 0, 0, 0, {0x46}, 1},
	{"LSR", MODE_ZP_X,     lsr_zp_x,  6, 0, 0, 0, {0x56}, 1},
	{"LSR", MODE_ABSOLUTE_X, lsr_abs_x, 7, 0, 0, 0, {0x5E}, 1},
	{"LSR", MODE_IMPLIED,   lsra,      2, 0, 0, 0, {0x4A}, 1},
	{"ROL", MODE_ABSOLUTE,   rol_abs,   6, 0, 0, 0, {0x2E}, 1},
	{"ROL", MODE_ZP,        rol_zp,    5, 0, 0, 0, {0x26}, 1},
	{"ROL", MODE_ZP_X,     rol_zp_x,  6, 0, 0, 0, {0x36}, 1},
	{"ROL", MODE_ABSOLUTE_X, rol_abs_x, 7, 0, 0, 0, {0x3E}, 1},
	{"ROL", MODE_IMPLIED,   rola,      2, 0, 0, 0, {0x2A}, 1},
	{"ROR", MODE_ABSOLUTE,   ror_abs,   6, 0, 0, 0, {0x6E}, 1},
	{"ROR", MODE_ZP,        ror_zp,    5, 0, 0, 0, {0x66}, 1},
	{"ROR", MODE_ZP_X,     ror_zp_x,  6, 0, 0, 0, {0x76}, 1},
	{"ROR", MODE_ABSOLUTE_X, ror_abs_x, 7, 0, 0, 0, {0x7E}, 1},
	{"ROR", MODE_IMPLIED,   rora,      2, 0, 0, 0, {0x6A}, 1},
	{"AND", MODE_IMMEDIATE,  and_imm,   2, 0, 0, 0, {0x29}, 1},
	{"AND", MODE_ABSOLUTE,   and_abs,   4, 0, 0, 0, {0x2D}, 1},
	{"AND", MODE_ZP,        and_zp,    3, 0, 0, 0, {0x25}, 1},
	{"AND", MODE_ZP_X,     and_zp_x,  4, 0, 0, 0, {0x35}, 1},
	{"AND", MODE_ABSOLUTE_X, and_abs_x, 4, 0, 0, 0, {0x3D}, 1},
	{"AND", MODE_ABSOLUTE_Y, and_abs_y, 4, 0, 0, 0, {0x39}, 1},
	{"AND", MODE_INDIRECT_X, and_ind_x, 6, 0, 0, 0, {0x21}, 1},
	{"AND", MODE_INDIRECT_Y, and_ind_y, 5, 0, 0, 0, {0x31}, 1},
	{"EOR", MODE_IMMEDIATE,  eor_imm,   2, 0, 0, 0, {0x49}, 1},
	{"EOR", MODE_ABSOLUTE,   eor_abs,   4, 0, 0, 0, {0x4D}, 1},
	{"EOR", MODE_ZP,        eor_zp,    3, 0, 0, 0, {0x45}, 1},
	{"EOR", MODE_ZP_X,     eor_zp_x,  4, 0, 0, 0, {0x55}, 1},
	{"EOR", MODE_ABSOLUTE_X, eor_abs_x, 4, 0, 0, 0, {0x5D}, 1},
	{"EOR", MODE_ABSOLUTE_Y, eor_abs_y, 4, 0, 0, 0, {0x59}, 1},
	{"EOR", MODE_INDIRECT_X, eor_ind_x, 6, 0, 0, 0, {0x41}, 1},
	{"EOR", MODE_INDIRECT_Y, eor_ind_y, 5, 0, 0, 0, {0x51}, 1},
	{"ORA", MODE_IMMEDIATE,  ora_imm,   2, 0, 0, 0, {0x09}, 1},
	{"ORA", MODE_ABSOLUTE,   ora_abs,   4, 0, 0, 0, {0x0D}, 1},
	{"ORA", MODE_ZP,        ora_zp,    3, 0, 0, 0, {0x05}, 1},
	{"ORA", MODE_ZP_X,     ora_zp_x,  4, 0, 0, 0, {0x15}, 1},
	{"ORA", MODE_ABSOLUTE_X, ora_abs_x, 4, 0, 0, 0, {0x1D}, 1},
	{"ORA", MODE_ABSOLUTE_Y, ora_abs_y, 4, 0, 0, 0, {0x19}, 1},
	{"ORA", MODE_INDIRECT_X, ora_ind_x, 6, 0, 0, 0, {0x01}, 1},
	{"ORA", MODE_INDIRECT_Y, ora_ind_y, 5, 0, 0, 0, {0x11}, 1},
	{"BIT", MODE_ZP,       bit_zp,  3, 0, 0, 0, {0x24}, 1},
	{"BIT", MODE_ABSOLUTE, bit_abs, 4, 0, 0, 0, {0x2C}, 1},
	{"JMP", MODE_ABSOLUTE,  jmp_abs, 3, 0, 0, 0, {0x4C}, 1},
	{"JMP", MODE_INDIRECT, jmp_ind, 5, 0, 0, 0, {0x6C}, 1},
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
};

int OPCODES_6502_COUNT = sizeof(opcodes_6502) / sizeof(opcodes_6502[0]);
