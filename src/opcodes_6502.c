#include "opcodes.h"

void lda_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = arg & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void lda_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void lda_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg + cpu->x);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void lda_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg + cpu->y);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void lda_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void lda_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, (arg + cpu->x) & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void lda_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a = mem_read(mem, addr);
	update_nz(cpu, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void lda_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a = mem_read(mem, addr + cpu->y);
	update_nz(cpu, cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void ldx_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = arg & 0xFF;
	update_nz(cpu, cpu->x);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void ldx_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, arg);
	update_nz(cpu, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldx_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, arg + cpu->y);
	update_nz(cpu, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldx_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->x);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void ldx_zp_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = mem_read(mem, (arg + cpu->y) & 0xFF);
	update_nz(cpu, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void ldy_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = arg & 0xFF;
	update_nz(cpu, cpu->y);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void ldy_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, arg);
	update_nz(cpu, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldy_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, arg + cpu->x);
	update_nz(cpu, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ldy_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->y);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void ldy_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = mem_read(mem, (arg + cpu->x) & 0xFF);
	update_nz(cpu, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sta_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void sta_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->x, cpu->a);
	cpu->cycles += 5;
	cpu->pc += 3;
}

void sta_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->y, cpu->a);
	cpu->cycles += 5;
	cpu->pc += 3;
}

void sta_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void sta_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->x) & 0xFF, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sta_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	mem_write(mem, addr, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void sta_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	mem_write(mem, addr + cpu->y, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void stx_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void stx_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->x);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void stx_zp_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->y) & 0xFF, cpu->x);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void sty_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void sty_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->y);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void sty_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->x) & 0xFF, cpu->y);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void adc_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void adc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void adc_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void adc_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->y);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void adc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

void adc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

void adc_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

void adc_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr + cpu->y);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

void sbc_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->a - val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (~val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void sbc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int result = cpu->a - val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (~val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void cmp_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->a - val;
	set_flag(cpu, FLAG_C, cpu->a >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void cmp_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int result = cpu->a - val;
	set_flag(cpu, FLAG_C, cpu->a >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void cpx_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->x - val;
	set_flag(cpu, FLAG_C, cpu->x >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void cpy_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->y - val;
	set_flag(cpu, FLAG_C, cpu->y >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void inc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg) + 1) & 0xFF;
	mem_write(mem, arg, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void ina(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = (cpu->a + 1) & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void inx(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = (cpu->x + 1) & 0xFF;
	update_nz(cpu, cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void iny(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = (cpu->y + 1) & 0xFF;
	update_nz(cpu, cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void dec_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg) - 1) & 0xFF;
	mem_write(mem, arg, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void dea(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = (cpu->a - 1) & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void dex(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = (cpu->x - 1) & 0xFF;
	update_nz(cpu, cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void dey(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = (cpu->y - 1) & 0xFF;
	update_nz(cpu, cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void asl_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	set_flag(cpu, FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void asla(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_C, cpu->a & 0x80);
	cpu->a = (cpu->a << 1) & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void lsr_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	set_flag(cpu, FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void lsra(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_C, cpu->a & 0x01);
	cpu->a >>= 1;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void rol_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void rola(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, cpu->a & 0x80);
	cpu->a = ((cpu->a << 1) | c) & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void ror_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 3;
}

void rora(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, cpu->a & 0x01);
	cpu->a = ((cpu->a >> 1) | (c << 7)) & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void and_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= arg & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void and_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void eor_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= arg & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void eor_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void ora_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= arg & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 2;
}

void ora_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void bit_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
	set_flag(cpu, FLAG_N, val & 0x80);
	set_flag(cpu, FLAG_V, val & 0x40);
	cpu->cycles += 4;
	cpu->pc += 3;
}

void jmp_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->pc = arg;
}

void jsr_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, 0x100 + cpu->s, (ret >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, ret & 0xFF);
	cpu->s--;
	cpu->pc = arg;
}

void rts(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	unsigned short ret = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	ret |= mem_read(mem, 0x100 + cpu->s) << 8;
	cpu->pc = ret + 1;
}

void bra(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	signed char offset = arg & 0xFF;
	cpu->pc += 2 + offset;
}

void bcc(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_C))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bcs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_C))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void beq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_Z))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bne(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_Z))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bmi(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_N))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bpl(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_N))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bvs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_V))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void bvc(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_V))
		bra(cpu, mem, arg);
	else {
		cpu->cycles += 2;
		cpu->pc += 2;
	}
}

void clc(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_C, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void sec(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_C, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void cld(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_D, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void sed(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_D, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void cli(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_I, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void sei(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_I, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void clv(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_V, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tax(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = cpu->a;
	update_nz(cpu, cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void txa(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->x;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tay(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = cpu->a;
	update_nz(cpu, cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tya(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->y;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void tsx(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = cpu->s;
	update_nz(cpu, cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

void txs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s = cpu->x;
	cpu->cycles += 2;
	cpu->pc += 1;
}

void pha(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, cpu->a);
	cpu->s--;
	cpu->cycles += 3;
	cpu->pc += 1;
}

void pla(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->a = mem_read(mem, 0x100 + cpu->s);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 1;
}

void php(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, cpu->p | FLAG_B);
	cpu->s--;
	cpu->cycles += 3;
	cpu->pc += 1;
}

void plp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->p = mem_read(mem, 0x100 + cpu->s);
	cpu->cycles += 4;
	cpu->pc += 1;
}

void brk(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 7;
	cpu->pc += 2;
	mem_write(mem, 0x100 + cpu->s, (cpu->pc >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, cpu->pc & 0xFF);
	cpu->s--;
	set_flag(cpu, FLAG_B, 1);
	mem_write(mem, 0x100 + cpu->s, cpu->p);
	cpu->s--;
	set_flag(cpu, FLAG_I, 1);
}

void rti(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->p = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	unsigned short ret = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	ret |= mem_read(mem, 0x100 + cpu->s) << 8;
	cpu->pc = ret;
}

void nop(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 2;
	cpu->pc += 1;
}

opcode_handler_t opcodes_6502[] = {
	{"LDA", MODE_IMMEDIATE, lda_imm, 2},
	{"LDA", MODE_ABSOLUTE, lda_abs, 4},
	{"LDA", MODE_ABSOLUTE_X, lda_abs_x, 4},
	{"LDA", MODE_ABSOLUTE_Y, lda_abs_y, 4},
	{"LDA", MODE_ZP, lda_zp, 3},
	{"LDA", MODE_ZP_X, lda_zp_x, 4},
	{"LDA", MODE_INDIRECT_X, lda_ind_x, 6},
	{"LDA", MODE_INDIRECT_Y, lda_ind_y, 5},
	{"LDX", MODE_IMMEDIATE, ldx_imm, 2},
	{"LDX", MODE_ABSOLUTE, ldx_abs, 4},
	{"LDX", MODE_ABSOLUTE_Y, ldx_abs_y, 4},
	{"LDX", MODE_ZP, ldx_zp, 3},
	{"LDX", MODE_ZP_Y, ldx_zp_y, 4},
	{"LDY", MODE_IMMEDIATE, ldy_imm, 2},
	{"LDY", MODE_ABSOLUTE, ldy_abs, 4},
	{"LDY", MODE_ABSOLUTE_X, ldy_abs_x, 4},
	{"LDY", MODE_ZP, ldy_zp, 3},
	{"LDY", MODE_ZP_X, ldy_zp_x, 4},
	{"STA", MODE_ABSOLUTE, sta_abs, 4},
	{"STA", MODE_ABSOLUTE_X, sta_abs_x, 5},
	{"STA", MODE_ABSOLUTE_Y, sta_abs_y, 5},
	{"STA", MODE_ZP, sta_zp, 3},
	{"STA", MODE_ZP_X, sta_zp_x, 4},
	{"STA", MODE_INDIRECT_X, sta_ind_x, 6},
	{"STA", MODE_INDIRECT_Y, sta_ind_y, 6},
	{"STX", MODE_ABSOLUTE, stx_abs, 4},
	{"STX", MODE_ZP, stx_zp, 3},
	{"STX", MODE_ZP_Y, stx_zp_y, 4},
	{"STY", MODE_ABSOLUTE, sty_abs, 4},
	{"STY", MODE_ZP, sty_zp, 3},
	{"STY", MODE_ZP_X, sty_zp_x, 4},
	{"ADC", MODE_IMMEDIATE, adc_imm, 2},
	{"ADC", MODE_ABSOLUTE, adc_abs, 4},
	{"ADC", MODE_ABSOLUTE_X, adc_abs_x, 4},
	{"ADC", MODE_ABSOLUTE_Y, adc_abs_y, 4},
	{"ADC", MODE_ZP, adc_zp, 3},
	{"ADC", MODE_ZP_X, adc_zp_x, 4},
	{"ADC", MODE_INDIRECT_X, adc_ind_x, 6},
	{"ADC", MODE_INDIRECT_Y, adc_ind_y, 5},
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
	{"DEY", MODE_IMPLIED, dey, 2},
	{"ASL", MODE_ABSOLUTE, asl_abs, 6},
	{"ASL", MODE_IMPLIED, asla, 2},
	{"LSR", MODE_ABSOLUTE, lsr_abs, 6},
	{"LSR", MODE_IMPLIED, lsra, 2},
	{"ROL", MODE_ABSOLUTE, rol_abs, 6},
	{"ROL", MODE_IMPLIED, rola, 2},
	{"ROR", MODE_ABSOLUTE, ror_abs, 6},
	{"ROR", MODE_IMPLIED, rora, 2},
	{"AND", MODE_IMMEDIATE, and_imm, 2},
	{"AND", MODE_ABSOLUTE, and_abs, 4},
	{"EOR", MODE_IMMEDIATE, eor_imm, 2},
	{"EOR", MODE_ABSOLUTE, eor_abs, 4},
	{"ORA", MODE_IMMEDIATE, ora_imm, 2},
	{"ORA", MODE_ABSOLUTE, ora_abs, 4},
	{"BIT", MODE_ABSOLUTE, bit_abs, 4},
	{"JMP", MODE_ABSOLUTE, jmp_abs, 3},
	{"JSR", MODE_ABSOLUTE, jsr_abs, 6},
	{"RTS", MODE_IMPLIED, rts, 6},
	{"BRA", MODE_RELATIVE, bra, 2},
	{"BCC", MODE_RELATIVE, bcc, 2},
	{"BCS", MODE_RELATIVE, bcs, 2},
	{"BEQ", MODE_RELATIVE, beq, 2},
	{"BNE", MODE_RELATIVE, bne, 2},
	{"BMI", MODE_RELATIVE, bmi, 2},
	{"BPL", MODE_RELATIVE, bpl, 2},
	{"BVS", MODE_RELATIVE, bvs, 2},
	{"BVC", MODE_RELATIVE, bvc, 2},
	{"CLC", MODE_IMPLIED, clc, 2},
	{"SEC", MODE_IMPLIED, sec, 2},
	{"CLD", MODE_IMPLIED, cld, 2},
	{"SED", MODE_IMPLIED, sed, 2},
	{"CLI", MODE_IMPLIED, cli, 2},
	{"SEI", MODE_IMPLIED, sei, 2},
	{"CLV", MODE_IMPLIED, clv, 2},
	{"TAX", MODE_IMPLIED, tax, 2},
	{"TXA", MODE_IMPLIED, txa, 2},
	{"TAY", MODE_IMPLIED, tay, 2},
	{"TYA", MODE_IMPLIED, tya, 2},
	{"TSX", MODE_IMPLIED, tsx, 2},
	{"TXS", MODE_IMPLIED, txs, 2},
	{"PHA", MODE_IMPLIED, pha, 3},
	{"PLA", MODE_IMPLIED, pla, 4},
	{"PHP", MODE_IMPLIED, php, 3},
	{"PLP", MODE_IMPLIED, plp, 4},
	{"BRK", MODE_IMPLIED, brk, 7},
	{"RTI", MODE_IMPLIED, rti, 6},
	{"NOP", MODE_IMPLIED, nop, 2},
};

int OPCODES_6502_COUNT = sizeof(opcodes_6502) / sizeof(opcodes_6502[0]);
