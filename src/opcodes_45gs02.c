#include "opcodes.h"

static void ldz_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = arg & 0xFF;
	update_nz(cpu, cpu->z);
	cpu->cycles += 2;
	cpu->pc += 2;
}

static void ldz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = mem_read(mem, arg);
	update_nz(cpu, cpu->z);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void ldz_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->z);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void stz_z_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->z);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void stz_z_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->z);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void inz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z++;
	update_nz(cpu, cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void dez(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z--;
	update_nz(cpu, cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void cpz_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->z - val;
	set_flag(cpu, FLAG_C, cpu->z >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

static void cpz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int result = cpu->z - val;
	set_flag(cpu, FLAG_C, cpu->z >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void cpz_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->z - val;
	set_flag(cpu, FLAG_C, cpu->z >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void tzx(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->x = cpu->z;
	update_nz(cpu, cpu->x);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tzy(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = cpu->z;
	update_nz(cpu, cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void txz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = cpu->x;
	update_nz(cpu, cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tyz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = cpu->y;
	update_nz(cpu, cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void sta_zp_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->y) & 0xFF, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void lda_zp_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = mem_read(mem, (arg + cpu->y) & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void tsy(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = cpu->s;
	update_nz(cpu, cpu->y);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tys(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s = cpu->y;
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void taz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = cpu->a;
	update_nz(cpu, cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tza(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->z;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void cle(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* E flag is not explicitly in cpu_t, but we can clear it if it was there */
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void see(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void neg(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = (unsigned char)((-cpu->a) & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asr_acc(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_C, cpu->a & 0x01);
	cpu->a = (unsigned char)((cpu->a >> 1) | (cpu->a & 0x80));
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void map(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* MAP is complex, for now just increment PC and cycles */
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asr_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	set_flag(cpu, FLAG_C, val & 0x01);
	val = (unsigned char)((val >> 1) | (val & 0x80));
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void asr_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	set_flag(cpu, FLAG_C, val & 0x01);
	val = (unsigned char)((val >> 1) | (val & 0x80));
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void tab(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->b = cpu->a;
	update_nz(cpu, cpu->b);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tba(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->b;
	update_nz(cpu, cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asw(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	set_flag(cpu, FLAG_C, val & 0x8000);
	val = (val << 1) & 0xFFFF;
	mem_write(mem, arg, val & 0xFF);
	mem_write(mem, arg + 1, (val >> 8) & 0xFF);
	update_nz(cpu, val & 0xFF); /* NZ updated based on low byte? Or 16-bit? Book says 8-bit usually */
	cpu->cycles += 6;
	cpu->pc += 3;
}

static void row(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	int old_c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x01);
	val = (val >> 1) | (old_c << 15);
	mem_write(mem, arg, val & 0xFF);
	mem_write(mem, arg + 1, (val >> 8) & 0xFF);
	update_nz(cpu, val & 0xFF);
	cpu->cycles += 6;
	cpu->pc += 3;
}

static void dew(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	val--;
	mem_write(mem, arg & 0xFF, val & 0xFF);
	mem_write(mem, (arg + 1) & 0xFF, (val >> 8) & 0xFF);
	update_nz(cpu, val == 0 ? 0 : 1); /* Actually Z is set if 16-bit is 0 */
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void inw(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	val++;
	mem_write(mem, arg & 0xFF, val & 0xFF);
	mem_write(mem, (arg + 1) & 0xFF, (val >> 8) & 0xFF);
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void phw_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, (arg >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, arg & 0xFF);
	cpu->s--;
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void phw_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	mem_write(mem, 0x100 + cpu->s, (val >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, val & 0xFF);
	cpu->s--;
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void phz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, cpu->z);
	cpu->s--;
	cpu->cycles += 3;
	cpu->pc += 1;
}

static void plz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	cpu->z = mem_read(mem, 0x100 + cpu->s);
	update_nz(cpu, cpu->z);
	cpu->cycles += 4;
	cpu->pc += 1;
}

static void ldz_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = mem_read(mem, arg + cpu->x);
	update_nz(cpu, cpu->z);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void rtn(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* RTN: Return from Nested Subroutine */
	/* Similar to RTS but might have different behavior on 45GS02 */
	/* For now, implement as RTS with different cycles */
	cpu->s++;
	unsigned short lo = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	unsigned short hi = mem_read(mem, 0x100 + cpu->s);
	cpu->pc = (hi << 8) | lo;
	cpu->pc++; /* Return address is stored as addr-1 */
	cpu->cycles += 6;
}

static void bra_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* 16-bit relative branch */
	cpu->pc += (short)arg + 3;
	cpu->cycles += 4;
}

static void asl_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	set_flag(cpu, FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void asl_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	set_flag(cpu, FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void lsr_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	set_flag(cpu, FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void lsr_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	set_flag(cpu, FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void rol_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void rol_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void ror_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void ror_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void inc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) + 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void inc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, (arg + cpu->x) & 0xFF) + 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void dec_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) - 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void dec_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, (arg + cpu->x) & 0xFF) - 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	update_nz(cpu, val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void adc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void adc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void sbc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->a - val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (~val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void sbc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int result = cpu->a - val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (~val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void and_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void and_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, (arg + cpu->x) & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void eor_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void eor_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, (arg + cpu->x) & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void ora_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void ora_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, (arg + cpu->x) & 0xFF);
	update_nz(cpu, cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void cmp_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->a - val;
	set_flag(cpu, FLAG_C, cpu->a >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void cmp_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int result = cpu->a - val;
	set_flag(cpu, FLAG_C, cpu->a >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void cpx_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->x - val;
	set_flag(cpu, FLAG_C, cpu->x >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void cpy_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->y - val;
	set_flag(cpu, FLAG_C, cpu->y >= val);
	update_nz(cpu, result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void stz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg, cpu->z);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void stz_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->x, cpu->z);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void stz_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg & 0xFF, cpu->z);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void stz_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, (arg + cpu->x) & 0xFF, cpu->z);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void bra_rel(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->pc += (signed char)arg + 2;
	cpu->cycles += 3;
}

static void wai(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 3;
	cpu->pc += 1;
}

static void stp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 3;
	cpu->pc += 1;
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
extern void adc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void adc_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sbc_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sbc_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sbc_zp_indirect(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void sbc_abs_indirect_y(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cmp_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cmp_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void cmp_zp_indirect(cpu_t *cpu, memory_t *mem, unsigned short arg);
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
extern void bit_imm(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bit_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bit_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void trb_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void trb_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void tsb_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void tsb_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb0(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb1(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb2(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb3(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb4(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb5(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb6(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rmb7(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb0(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb1(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb2(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb3(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb4(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb5(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb6(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void smb7(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr0(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr1(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr2(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr3(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr4(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr5(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr6(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbr7(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs0(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs1(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs2(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs3(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs4(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs5(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs6(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bbs7(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stz_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stz_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void stz_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void bra_rel(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void brl(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void jmp_abs(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void jmp_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg);
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
extern void phx(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void plx(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void phy(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void ply(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void brk(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void rti(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void eom(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void nop(cpu_t *cpu, memory_t *mem, unsigned short arg);

opcode_handler_t opcodes_45gs02[] = {
	{"LDA", MODE_IMMEDIATE, lda_imm, 2},
	{"LDA", MODE_ABSOLUTE, lda_abs, 4},
	{"LDA", MODE_ABSOLUTE_X, lda_abs_x, 4},
	{"LDA", MODE_ABSOLUTE_Y, lda_abs_y, 4},
	{"LDA", MODE_ZP, lda_zp, 3},
	{"LDA", MODE_ZP_X, lda_zp_x, 4},
	{"LDA", MODE_ZP_Y, lda_zp_y, 4},
	{"ASW", MODE_ABSOLUTE, asw, 6},
	{"ROW", MODE_ABSOLUTE, row, 6},
	{"DEW", MODE_ZP, dew, 6},
	{"INW", MODE_ZP, inw, 6},
	{"PHW", MODE_IMMEDIATE, phw_imm, 4},
	{"PHW", MODE_ABSOLUTE, phw_abs, 5},
	{"PHZ", MODE_IMPLIED, phz, 3},
	{"PLZ", MODE_IMPLIED, plz, 4},
	{"LDZ", MODE_ABSOLUTE_X, ldz_abs_x, 4},
	{"TSY", MODE_IMPLIED, tsy, 2},
	{"TYS", MODE_IMPLIED, tys, 2},
	{"TAZ", MODE_IMPLIED, taz, 2},
	{"TZA", MODE_IMPLIED, tza, 2},
	{"CLE", MODE_IMPLIED, cle, 2},
	{"SEE", MODE_IMPLIED, see, 2},
	{"NEG", MODE_IMPLIED, neg, 2},
	{"TAB", MODE_IMPLIED, tab, 2},
	{"TBA", MODE_IMPLIED, tba, 2},
	{"RTN", MODE_IMMEDIATE, rtn, 6},
	{"BSR", MODE_RELATIVE, bra_relfar, 4},
	{"ASR", MODE_IMPLIED, asr_acc, 2},
	{"ASR", MODE_ZP, asr_zp, 5},
	{"ASR", MODE_ZP_X, asr_zp_x, 6},
	{"MAP", MODE_IMPLIED, map, 2},
	{"LDA", MODE_INDIRECT_X, lda_ind_x, 6},
	{"LDA", MODE_INDIRECT_Y, lda_ind_y, 5},
	{"LDZ", MODE_IMMEDIATE, ldz_imm, 2},
	{"LDZ", MODE_ABSOLUTE, ldz_abs, 4},
	{"LDZ", MODE_ZP, ldz_zp, 3},
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
	{"STA", MODE_ZP_Y, sta_zp_y, 4},
	{"STA", MODE_INDIRECT_X, sta_ind_x, 6},
	{"STA", MODE_INDIRECT_Y, sta_ind_y, 6},
	{"STZ", MODE_ABSOLUTE, stz_abs, 4},
	{"STZ", MODE_ZP, stz_zp, 3},
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
	{"SBC", MODE_ZP, sbc_zp, 3},
	{"SBC", MODE_ZP_X, sbc_zp_x, 4},
	{"SBC", MODE_ZP_INDIRECT, sbc_zp_indirect, 5},
	{"SBC", MODE_ABS_INDIRECT_Y, sbc_abs_indirect_y, 6},
	{"CMP", MODE_IMMEDIATE, cmp_imm, 2},
	{"CMP", MODE_ABSOLUTE, cmp_abs, 4},
	{"CMP", MODE_ZP, cmp_zp, 3},
	{"CMP", MODE_ZP_X, cmp_zp_x, 4},
	{"CMP", MODE_ZP_INDIRECT, cmp_zp_indirect, 5},
	{"CPZ", MODE_IMMEDIATE, cpz_imm, 2},
	{"CPZ", MODE_ABSOLUTE, cpz_abs, 4},
	{"CPZ", MODE_ZP, cpz_zp, 3},
	{"CPX", MODE_IMMEDIATE, cpx_imm, 2},
	{"CPX", MODE_ZP, cpx_zp, 3},
	{"CPY", MODE_IMMEDIATE, cpy_imm, 2},
	{"CPY", MODE_ZP, cpy_zp, 3},
	{"INC", MODE_ABSOLUTE, inc_abs, 6},
	{"INC", MODE_ZP, inc_zp, 5},
	{"INC", MODE_ZP_X, inc_zp_x, 6},
	{"INA", MODE_IMPLIED, ina, 2},
	{"INX", MODE_IMPLIED, inx, 2},
	{"INY", MODE_IMPLIED, iny, 2},
	{"INZ", MODE_IMPLIED, inz, 2},
	{"DEC", MODE_ABSOLUTE, dec_abs, 6},
	{"DEC", MODE_ZP, dec_zp, 5},
	{"DEC", MODE_ZP_X, dec_zp_x, 6},
	{"DEA", MODE_IMPLIED, dea, 2},
	{"DEX", MODE_IMPLIED, dex, 2},
	{"DEY", MODE_IMPLIED, dey, 2},
	{"DEZ", MODE_IMPLIED, dez, 2},
	{"ASL", MODE_ABSOLUTE, asl_abs, 6},
	{"ASL", MODE_IMPLIED, asla, 2},
	{"ASL", MODE_ZP, asl_zp, 5},
	{"ASL", MODE_ZP_X, asl_zp_x, 6},
	{"LSR", MODE_ABSOLUTE, lsr_abs, 6},
	{"LSR", MODE_IMPLIED, lsra, 2},
	{"LSR", MODE_ZP, lsr_zp, 5},
	{"LSR", MODE_ZP_X, lsr_zp_x, 6},
	{"ROL", MODE_ABSOLUTE, rol_abs, 6},
	{"ROL", MODE_IMPLIED, rola, 2},
	{"ROL", MODE_ZP, rol_zp, 5},
	{"ROL", MODE_ZP_X, rol_zp_x, 6},
	{"ROR", MODE_ABSOLUTE, ror_abs, 6},
	{"ROR", MODE_IMPLIED, rora, 2},
	{"ROR", MODE_ZP, ror_zp, 5},
	{"ROR", MODE_ZP_X, ror_zp_x, 6},
	{"AND", MODE_IMMEDIATE, and_imm, 2},
	{"AND", MODE_ABSOLUTE, and_abs, 4},
	{"AND", MODE_ZP, and_zp, 3},
	{"AND", MODE_ZP_X, and_zp_x, 4},
	{"EOR", MODE_IMMEDIATE, eor_imm, 2},
	{"EOR", MODE_ABSOLUTE, eor_abs, 4},
	{"EOR", MODE_ZP, eor_zp, 3},
	{"EOR", MODE_ZP_X, eor_zp_x, 4},
	{"ORA", MODE_IMMEDIATE, ora_imm, 2},
	{"ORA", MODE_ABSOLUTE, ora_abs, 4},
	{"ORA", MODE_ZP, ora_zp, 3},
	{"ORA", MODE_ZP_X, ora_zp_x, 4},
	{"BIT", MODE_ABSOLUTE, bit_abs, 4},
	{"BIT", MODE_IMMEDIATE, bit_imm, 2},
	{"BIT", MODE_ZP_X, bit_zp_x, 4},
	{"BIT", MODE_ABSOLUTE_X, bit_abs_x, 4},
	{"TRB", MODE_ZP, trb_zp, 5},
	{"TRB", MODE_ABSOLUTE, trb_abs, 6},
	{"TSB", MODE_ZP, tsb_zp, 5},
	{"TSB", MODE_ABSOLUTE, tsb_abs, 6},
	{"RMB0", MODE_ZP, rmb0, 5},
	{"RMB1", MODE_ZP, rmb1, 5},
	{"RMB2", MODE_ZP, rmb2, 5},
	{"RMB3", MODE_ZP, rmb3, 5},
	{"RMB4", MODE_ZP, rmb4, 5},
	{"RMB5", MODE_ZP, rmb5, 5},
	{"RMB6", MODE_ZP, rmb6, 5},
	{"RMB7", MODE_ZP, rmb7, 5},
	{"SMB0", MODE_ZP, smb0, 5},
	{"SMB1", MODE_ZP, smb1, 5},
	{"SMB2", MODE_ZP, smb2, 5},
	{"SMB3", MODE_ZP, smb3, 5},
	{"SMB4", MODE_ZP, smb4, 5},
	{"SMB5", MODE_ZP, smb5, 5},
	{"SMB6", MODE_ZP, smb6, 5},
	{"SMB7", MODE_ZP, smb7, 5},
	{"BBR0", MODE_ZP, bbr0, 5},
	{"BBR1", MODE_ZP, bbr1, 5},
	{"BBR2", MODE_ZP, bbr2, 5},
	{"BBR3", MODE_ZP, bbr3, 5},
	{"BBR4", MODE_ZP, bbr4, 5},
	{"BBR5", MODE_ZP, bbr5, 5},
	{"BBR6", MODE_ZP, bbr6, 5},
	{"BBR7", MODE_ZP, bbr7, 5},
	{"BBS0", MODE_ZP, bbs0, 5},
	{"BBS1", MODE_ZP, bbs1, 5},
	{"BBS2", MODE_ZP, bbs2, 5},
	{"BBS3", MODE_ZP, bbs3, 5},
	{"BBS4", MODE_ZP, bbs4, 5},
	{"BBS5", MODE_ZP, bbs5, 5},
	{"BBS6", MODE_ZP, bbs6, 5},
	{"BBS7", MODE_ZP, bbs7, 5},
	{"STZ", MODE_ABSOLUTE, stz_abs, 4},
	{"STZ", MODE_ABSOLUTE_X, stz_abs_x, 5},
	{"STZ", MODE_ZP, stz_zp, 3},
	{"STZ", MODE_ZP_X, stz_zp_x, 4},
	{"BRA", MODE_RELATIVE, bra_rel, 2},
	{"BRA", MODE_RELATIVE, bra_relfar, 3},
	{"BRL", MODE_RELATIVE, bra_relfar, 3},
	{"JMP", MODE_ABSOLUTE, jmp_abs, 3},
	{"JMP", MODE_INDIRECT_X, jmp_ind_x, 6},
	{"JSR", MODE_ABSOLUTE, jsr_abs, 6},
	{"RTS", MODE_IMPLIED, rts, 6},
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
	{"TZX", MODE_IMPLIED, tzx, 2},
	{"TXZ", MODE_IMPLIED, txz, 2},
	{"TZY", MODE_IMPLIED, tzy, 2},
	{"TYZ", MODE_IMPLIED, tyz, 2},
	{"PHA", MODE_IMPLIED, pha, 3},
	{"PLA", MODE_IMPLIED, pla, 4},
	{"PHX", MODE_IMPLIED, phx, 3},
	{"PLX", MODE_IMPLIED, plx, 4},
	{"PHY", MODE_IMPLIED, phy, 3},
	{"PLY", MODE_IMPLIED, ply, 4},
	{"PHP", MODE_IMPLIED, php, 3},
	{"PLP", MODE_IMPLIED, plp, 4},
	{"BRK", MODE_IMPLIED, brk, 7},
	{"RTI", MODE_IMPLIED, rti, 6},
	{"WAI", MODE_IMPLIED, wai, 3},
	{"STP", MODE_IMPLIED, stp, 3},
	{"EOM", MODE_IMPLIED, eom, 2},
	{"NOP", MODE_IMPLIED, nop, 2},
};

int OPCODES_45GS02_COUNT = sizeof(opcodes_45gs02) / sizeof(opcodes_45gs02[0]);
