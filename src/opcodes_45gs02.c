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
	set_flag(cpu, FLAG_E, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void see(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	set_flag(cpu, FLAG_E, 1);
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
	/* Decode MAP registers (45GS02/MEGA65 / C65 4510 specification):
	 *   A[7:0]  = lo_offset bits[15:8]  (lower-32KB offset, bits 8..15)
	 *   X[7:4]  = lo_select (one enable bit per 8KB block in $0000-$7FFF; bit0=$0000-$1FFF)
	 *   X[3:0]  = lo_offset bits[19:16]
	 *   Y[7:0]  = hi_offset bits[15:8]
	 *   Z[7:4]  = hi_select (one enable bit per 8KB block in $8000-$FFFF; bit0=$8000-$9FFF)
	 *   Z[3:0]  = hi_offset bits[19:16]
	 * Physical address = (virtual + offset) & 0xFFFFF (20-bit wrap per C65 spec). */
	unsigned int lo_off = ((unsigned int)(cpu->x & 0x0F) << 16) | ((unsigned int)cpu->a << 8);
	unsigned int hi_off = ((unsigned int)(cpu->z & 0x0F) << 16) | ((unsigned int)cpu->y << 8);
	unsigned char lo_sel = (cpu->x >> 4) & 0x0F;
	unsigned char hi_sel = (cpu->z >> 4) & 0x0F;
	for (int i = 0; i < 4; i++)
		mem->map_offset[i]     = (lo_sel & (1u << i)) ? lo_off : 0;
	for (int i = 0; i < 4; i++)
		mem->map_offset[4 + i] = (hi_sel & (1u << i)) ? hi_off : 0;
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

static void dew_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	val--;
	mem_write(mem, arg & 0xFF, val & 0xFF);
	mem_write(mem, (arg + 1) & 0xFF, (val >> 8) & 0xFF);
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void dew_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	val--;
	mem_write(mem, arg, val & 0xFF);
	mem_write(mem, arg + 1, (val >> 8) & 0xFF);
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 3;
}

static void inw_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	val++;
	mem_write(mem, arg & 0xFF, val & 0xFF);
	mem_write(mem, (arg + 1) & 0xFF, (val >> 8) & 0xFF);
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void inw_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	val++;
	mem_write(mem, arg, val & 0xFF);
	mem_write(mem, arg + 1, (val >> 8) & 0xFF);
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 3;
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

static void lda_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		cpu->a = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		cpu->a = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	update_nz(cpu, cpu->a);
	cpu->pc += 2;
}

static void sta_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		far_mem_write(mem, addr, cpu->a);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		mem_write(mem, (unsigned short)(addr + cpu->z), cpu->a);
		cpu->cycles += 5;
	}
	cpu->pc += 2;
}

static void adc_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		val = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		val = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	int result = cpu->a + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFF);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->pc += 2;
}

static void sbc_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		val = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		val = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	int result = cpu->a - val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	set_flag(cpu, FLAG_V, ((cpu->a ^ result) & (~val ^ result) & 0x80) != 0);
	cpu->a = result & 0xFF;
	update_nz(cpu, cpu->a);
	cpu->pc += 2;
}

static void cmp_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		val = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		val = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	int result = cpu->a - val;
	set_flag(cpu, FLAG_C, cpu->a >= val);
	update_nz(cpu, result & 0xFF);
	cpu->pc += 2;
}

static void ora_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		val = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		val = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	cpu->a |= val;
	update_nz(cpu, cpu->a);
	cpu->pc += 2;
}

static void and_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		val = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		val = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	cpu->a &= val;
	update_nz(cpu, cpu->a);
	cpu->pc += 2;
}

static void eor_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, arg & 0xFF)
			| ((unsigned int)mem_read(mem, (arg + 1) & 0xFF) << 8)
			| ((unsigned int)mem_read(mem, (arg + 2) & 0xFF) << 16)
			| ((unsigned int)mem_read(mem, (arg + 3) & 0xFF) << 24);
		addr += cpu->z;
		val = far_mem_read(mem, addr);
		cpu->cycles += 7;
	} else {
		unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
		val = mem_read(mem, (unsigned short)(addr + cpu->z));
		cpu->cycles += 5;
	}
	cpu->a ^= val;
	update_nz(cpu, cpu->a);
	cpu->pc += 2;
}

static void lda_sp_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ptr_addr = (0x100 + cpu->s + (arg & 0xFF)) & 0xFFFF;
	unsigned short addr = mem_read(mem, ptr_addr) | (mem_read(mem, (ptr_addr + 1) & 0xFFFF) << 8);
	cpu->a = mem_read(mem, addr + cpu->y);
	update_nz(cpu, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void sta_sp_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ptr_addr = (0x100 + cpu->s + (arg & 0xFF)) & 0xFFFF;
	unsigned short addr = mem_read(mem, ptr_addr) | (mem_read(mem, (ptr_addr + 1) & 0xFFFF) << 8);
	mem_write(mem, addr + cpu->y, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void jsr_ind(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, 0x100 + cpu->s, (ret >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, ret & 0xFF);
	cpu->s--;
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	cpu->pc = addr;
	cpu->cycles += 5;
}

static void jsr_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, 0x100 + cpu->s, (ret >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, ret & 0xFF);
	cpu->s--;
	unsigned short addr = mem_read(mem, arg + cpu->x) | (mem_read(mem, arg + cpu->x + 1) << 8);
	cpu->pc = addr;
	cpu->cycles += 5;
}

static void rtn(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->s++;
	unsigned short lo = mem_read(mem, 0x100 + cpu->s);
	cpu->s++;
	unsigned short hi = mem_read(mem, 0x100 + cpu->s);
	cpu->s += arg & 0xFF; /* discard stack-frame parameters */
	cpu->pc = (hi << 8) | lo;
	cpu->pc++; /* return address is stored as addr-1, same as RTS */
	cpu->cycles += 6;
}

static void bra_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* 16-bit relative branch */
	cpu->pc += (short)arg + 3;
	cpu->cycles += 4;
}

static void bsr_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* Push return address (last byte of BSR instruction) */
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, 0x100 + cpu->s, (ret >> 8) & 0xFF);
	cpu->s--;
	mem_write(mem, 0x100 + cpu->s, ret & 0xFF);
	cpu->s--;
	/* 16-bit relative branch */
	cpu->pc += (short)arg + 3;
	cpu->cycles += 4;
}

static void bcc_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_C)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bcs_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_C)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void beq_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_Z)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bne_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_Z)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bmi_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_N)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bpl_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_N)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bvc_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!get_flag(cpu, FLAG_V)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bvs_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (get_flag(cpu, FLAG_V)) {
		cpu->pc += (short)arg + 3;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
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

/* PHP on 45GS02: bit 5 is not the unused/always-1 bit (it has a different
 * meaning in this architecture), so we do not force it to 1. */
static void php_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, 0x100 + cpu->s, cpu->p | FLAG_B);
	cpu->s--;
	cpu->cycles += 3;
	cpu->pc += 1;
}

/* --- Quad (32-bit) Register Helpers ---
 * Q = {Z:Y:X:A} where A is bits 7:0 (LSB) and Z is bits 31:24 (MSB).
 */
static unsigned int get_q(cpu_t *cpu) {
	return (unsigned int)cpu->a |
	       ((unsigned int)cpu->x << 8) |
	       ((unsigned int)cpu->y << 16) |
	       ((unsigned int)cpu->z << 24);
}

static void set_q(cpu_t *cpu, unsigned int val) {
	cpu->a = val & 0xFF;
	cpu->x = (val >> 8) & 0xFF;
	cpu->y = (val >> 16) & 0xFF;
	cpu->z = (val >> 24) & 0xFF;
}

static void update_nz_q(cpu_t *cpu, unsigned int val) {
	set_flag(cpu, FLAG_Z, val == 0);
	set_flag(cpu, FLAG_N, (val >> 31) & 1);
}

/* Read 4 consecutive bytes from ZP (wrapping within ZP) as little-endian 32-bit */
static unsigned int mem_read32_zp(memory_t *mem, unsigned char addr) {
	return (unsigned int)mem_read(mem, (unsigned char)(addr)) |
	       ((unsigned int)mem_read(mem, (unsigned char)(addr + 1)) << 8) |
	       ((unsigned int)mem_read(mem, (unsigned char)(addr + 2)) << 16) |
	       ((unsigned int)mem_read(mem, (unsigned char)(addr + 3)) << 24);
}

/* Write 32-bit value as 4 consecutive bytes to ZP (wrapping within ZP) */
static void mem_write32_zp(memory_t *mem, unsigned char addr, unsigned int val) {
	mem_write(mem, (unsigned char)(addr),     val & 0xFF);
	mem_write(mem, (unsigned char)(addr + 1), (val >> 8) & 0xFF);
	mem_write(mem, (unsigned char)(addr + 2), (val >> 16) & 0xFF);
	mem_write(mem, (unsigned char)(addr + 3), (val >> 24) & 0xFF);
}

/* Read 4 consecutive bytes from absolute address (wrapping within 64KB) as little-endian 32-bit */
static unsigned int mem_read32_abs(memory_t *mem, unsigned short addr) {
	return (unsigned int)mem_read(mem, (unsigned short)(addr)) |
	       ((unsigned int)mem_read(mem, (unsigned short)(addr + 1)) << 8) |
	       ((unsigned int)mem_read(mem, (unsigned short)(addr + 2)) << 16) |
	       ((unsigned int)mem_read(mem, (unsigned short)(addr + 3)) << 24);
}

/* Write 32-bit value as 4 consecutive bytes to absolute address (wrapping within 64KB) */
static void mem_write32_abs(memory_t *mem, unsigned short addr, unsigned int val) {
	mem_write(mem, (unsigned short)(addr),     val & 0xFF);
	mem_write(mem, (unsigned short)(addr + 1), (val >> 8) & 0xFF);
	mem_write(mem, (unsigned short)(addr + 2), (val >> 16) & 0xFF);
	mem_write(mem, (unsigned short)(addr + 3), (val >> 24) & 0xFF);
}

/* --- Quad (32-bit) Instructions (NEG NEG prefix: $42 $42) --- */

static void ldq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	set_q(cpu, val);
	update_nz_q(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void ldq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg);
	set_q(cpu, val);
	update_nz_q(cpu, val);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void stq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), get_q(cpu));
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void stq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write32_abs(mem, arg, get_q(cpu));
	cpu->cycles += 5;
	cpu->pc += 3;
}

/* LDQ [$zp]  — flat 32-bit pointer at ZP, no Z offset, load Q (4 bytes) from far address. */
static void ldq_zp_ind(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		unsigned int val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		set_q(cpu, val);
		update_nz_q(cpu, val);
		cpu->cycles += 9;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		unsigned int val = mem_read32_abs(mem, ptr);
		set_q(cpu, val);
		update_nz_q(cpu, val);
		cpu->cycles += 7;
	}
	cpu->pc += 2;
}

/* STQ [$zp]  — flat 32-bit pointer at ZP, no Z offset, store Q (4 bytes) to far address. */
static void stq_zp_ind(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val = get_q(cpu);
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		far_mem_write(mem, addr,     val & 0xFF);
		far_mem_write(mem, addr + 1, (val >> 8)  & 0xFF);
		far_mem_write(mem, addr + 2, (val >> 16) & 0xFF);
		far_mem_write(mem, addr + 3, (val >> 24) & 0xFF);
		cpu->cycles += 9;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		mem_write32_abs(mem, ptr, val);
		cpu->cycles += 7;
	}
	cpu->pc += 2;
}

/* LDQ [$zp],Z  — flat 32-bit pointer at ZP, add Z, load Q (4 bytes) from far address. */
static void ldq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		unsigned int val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		set_q(cpu, val);
		update_nz_q(cpu, val);
		cpu->cycles += 9;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		unsigned int val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		set_q(cpu, val);
		update_nz_q(cpu, val);
		cpu->cycles += 7;
	}
	cpu->pc += 2;
}

/* STQ [$zp],Z  — flat 32-bit pointer at ZP, add Z, store Q (4 bytes) to far address.
 * Also handles the [zp] (no ,Z) form assembled as the same mode with Z=0. */
static void stq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val = get_q(cpu);
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		far_mem_write(mem, addr,     val & 0xFF);
		far_mem_write(mem, addr + 1, (val >> 8)  & 0xFF);
		far_mem_write(mem, addr + 2, (val >> 16) & 0xFF);
		far_mem_write(mem, addr + 3, (val >> 24) & 0xFF);
		cpu->cycles += 9;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		mem_write32_abs(mem, (unsigned short)(ptr + cpu->z), val);
		cpu->cycles += 7;
	}
	cpu->pc += 2;
}

static void adcq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	unsigned long long result = (unsigned long long)q + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFFFFFFFFUL);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	set_flag(cpu, FLAG_V, (~(q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void adcq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	unsigned long long result = (unsigned long long)q + val + get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, result > 0xFFFFFFFFUL);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	set_flag(cpu, FLAG_V, (~(q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void sbcq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	long long result = (long long)q - (long long)val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	set_flag(cpu, FLAG_V, ((q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void sbcq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	long long result = (long long)q - (long long)val - (1 - get_flag(cpu, FLAG_C));
	set_flag(cpu, FLAG_C, result >= 0);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	set_flag(cpu, FLAG_V, ((q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void cpq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	set_flag(cpu, FLAG_C, q >= val);
	update_nz_q(cpu, q - val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void cpq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	set_flag(cpu, FLAG_C, q >= val);
	update_nz_q(cpu, q - val);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void eorq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	unsigned int r = q ^ val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void eorq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	unsigned int r = q ^ val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void andq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	unsigned int r = q & val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void andq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	unsigned int r = q & val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void bitq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	set_flag(cpu, FLAG_Z, (q & val) == 0);
	set_flag(cpu, FLAG_N, (val >> 31) & 1);
	set_flag(cpu, FLAG_V, (val >> 30) & 1);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void bitq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	set_flag(cpu, FLAG_Z, (q & val) == 0);
	set_flag(cpu, FLAG_N, (val >> 31) & 1);
	set_flag(cpu, FLAG_V, (val >> 30) & 1);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void orq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	unsigned int r = q | val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void orq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	unsigned int r = q | val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void aslq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	set_flag(cpu, FLAG_C, (q >> 31) & 1);
	q <<= 1;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asrq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	set_flag(cpu, FLAG_C, q & 1);
	q = (q >> 1) | (q & 0x80000000); /* preserve sign bit */
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void lsrq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	set_flag(cpu, FLAG_C, q & 1);
	q >>= 1;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void rolq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, (q >> 31) & 1);
	q = (q << 1) | (unsigned int)c;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void rorq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	int c = get_flag(cpu, FLAG_C);
	set_flag(cpu, FLAG_C, q & 1);
	q = (q >> 1) | ((unsigned int)c << 31);
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void inq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu) + 1;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void deq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu) - 1;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
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
extern void bit_zp(cpu_t *cpu, memory_t *mem, unsigned short arg);
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
	{"LDA", MODE_ABSOLUTE_X, lda_abs_x, 4, 0, 0, 0, 0xBD},
	{"LDA", MODE_ABSOLUTE_Y, lda_abs_y, 4, 0, 0, 0, 0xB9},
	{"LDA", MODE_ZP, lda_zp, 3},
	{"LDA", MODE_ZP_X, lda_zp_x, 4, 0, 0, 0, 0xB5},
	{"LDA", MODE_ZP_Y, lda_zp_y, 4},
	{"ASW", MODE_ABSOLUTE, asw, 6},
	{"ROW", MODE_ABSOLUTE, row, 6},
	{"DEW", MODE_ZP, dew_zp, 6},
	{"DEW", MODE_ABSOLUTE, dew_abs, 6},
	{"INW", MODE_ZP, inw_zp, 6},
	{"INW", MODE_ABSOLUTE, inw_abs, 6},
	{"PHW", MODE_IMMEDIATE_WORD, phw_imm, 4, 0, 0, 0, 0xF4},
	{"PHW", MODE_ABSOLUTE, phw_abs, 5, 0, 0, 0, 0xFC},
	{"PHZ", MODE_IMPLIED, phz, 3, 0, 0, 0, 0xDB},
	{"PLZ", MODE_IMPLIED, plz, 4, 0, 0, 0, 0xFB},
	{"LDZ", MODE_ABSOLUTE_X, ldz_abs_x, 4, 0, 0, 0, 0xBB},
	{"TSY", MODE_IMPLIED, tsy, 2, 0, 0, 0, 0x0B},
	{"TYS", MODE_IMPLIED, tys, 2, 0, 0, 0, 0x2B},
	{"TAZ", MODE_IMPLIED, taz, 2, 0, 0, 0, 0x4B},
	{"TZA", MODE_IMPLIED, tza, 2, 0, 0, 0, 0x6B},
	{"CLE", MODE_IMPLIED, cle, 2, 0, 0, 0, 0x02},
	{"SEE", MODE_IMPLIED, see, 2, 0, 0, 0, 0x03},
	{"NEG", MODE_IMPLIED, neg, 2, 0, 0, 0, 0x42},
	{"TAB", MODE_IMPLIED, tab, 2, 0, 0, 0, 0x5B},
	{"TBA", MODE_IMPLIED, tba, 2, 0, 0, 0, 0x7B},
	{"RTN", MODE_IMMEDIATE, rtn, 6, 0, 0, 0, 0x62},
	{"BSR", MODE_RELATIVE_LONG, bsr_relfar, 4, 0, 0, 0, 0x63},
	{"LBSR", MODE_RELATIVE_LONG, bsr_relfar, 4, 0, 0, 0, 0x63},
	{"ASR", MODE_IMPLIED, asr_acc, 2, 0, 0, 0, 0x43},
	{"ASR", MODE_ZP, asr_zp, 5, 0, 0, 0, 0x44},
	{"ASR", MODE_ZP_X, asr_zp_x, 6, 0, 0, 0, 0x54},
	{"MAP", MODE_IMPLIED, map, 2, 0, 0, 0, 0x5C},
	{"LDA", MODE_INDIRECT_X, lda_ind_x, 6},
	{"LDA", MODE_INDIRECT_Y, lda_ind_y, 5, 0, 0, 0, 0xB1},
	{"LDZ", MODE_IMMEDIATE, ldz_imm, 2},
	{"LDZ", MODE_ABSOLUTE, ldz_abs, 4},
	{"LDZ", MODE_ZP, ldz_zp, 3},
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
	{"STA", MODE_ZP_Y, sta_zp_y, 4},
	{"STA", MODE_INDIRECT_X, sta_ind_x, 6, 0, 0, 0, 0x81},
	{"STA", MODE_INDIRECT_Y, sta_ind_y, 6, 0, 0, 0, 0x91},
	{"STZ", MODE_ABSOLUTE, stz_abs, 4, 0, 0, 0, 0x9C},
	{"STZ", MODE_ZP, stz_zp, 3, 0, 0, 0, 0x64},
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
	{"SBC", MODE_ZP, sbc_zp, 3},
	{"SBC", MODE_ZP_X, sbc_zp_x, 4, 0, 0, 0, 0xF5},
	{"LDA", MODE_ZP_INDIRECT_Z, lda_zp_ind_z, 5, 0, 0, 0, 0xB2},
	{"LDA", MODE_SP_INDIRECT_Y, lda_sp_ind_y, 6, 0, 0, 0, 0xE2},
	{"STA", MODE_ZP_INDIRECT_Z, sta_zp_ind_z, 5, 0, 0, 0, 0x92},
	{"STA", MODE_SP_INDIRECT_Y, sta_sp_ind_y, 6, 0, 0, 0, 0x82},
	{"ADC", MODE_ZP_INDIRECT_Z, adc_zp_ind_z, 5, 0, 0, 0, 0x72},
	{"SBC", MODE_ZP_INDIRECT_Z, sbc_zp_ind_z, 5, 0, 0, 0, 0xF2},
	{"CMP", MODE_ZP_INDIRECT_Z, cmp_zp_ind_z, 5, 0, 0, 0, 0xD2},
	{"ORA", MODE_ZP_INDIRECT_Z, ora_zp_ind_z, 5, 0, 0, 0, 0x12},
	{"AND", MODE_ZP_INDIRECT_Z, and_zp_ind_z, 5, 0, 0, 0, 0x32},
	{"EOR", MODE_ZP_INDIRECT_Z, eor_zp_ind_z, 5, 0, 0, 0, 0x52},
	{"SBC", MODE_ABS_INDIRECT_Y, sbc_abs_indirect_y, 6},
	{"CMP", MODE_IMMEDIATE, cmp_imm, 2},
	{"CMP", MODE_ABSOLUTE, cmp_abs, 4},
	{"CMP", MODE_ZP, cmp_zp, 3, 0, 0, 0, 0xD5},
	{"CMP", MODE_ZP_X, cmp_zp_x, 4},
	{"CPZ", MODE_IMMEDIATE, cpz_imm, 2},
	{"CPZ", MODE_ABSOLUTE, cpz_abs, 4, 0, 0, 0, 0xDC},
	{"CPZ", MODE_ZP, cpz_zp, 3, 0, 0, 0, 0xD4},
	{"CPX", MODE_IMMEDIATE, cpx_imm, 2},
	{"CPX", MODE_ZP, cpx_zp, 3},
	{"CPY", MODE_IMMEDIATE, cpy_imm, 2},
	{"CPY", MODE_ZP, cpy_zp, 3},
	{"INC", MODE_ABSOLUTE, inc_abs, 6},
	{"INC", MODE_ZP, inc_zp, 5},
	{"INC", MODE_ZP_X, inc_zp_x, 6, 0, 0, 0, 0xF6},
	{"INA", MODE_IMPLIED, ina, 2},
	{"INX", MODE_IMPLIED, inx, 2},
	{"INY", MODE_IMPLIED, iny, 2},
	{"INZ", MODE_IMPLIED, inz, 2, 0, 0, 0, 0x1B},
	{"DEC", MODE_ABSOLUTE, dec_abs, 6},
	{"DEC", MODE_ZP, dec_zp, 5},
	{"DEC", MODE_ZP_X, dec_zp_x, 6, 0, 0, 0, 0xD6},
	{"DEA", MODE_IMPLIED, dea, 2},
	{"DEX", MODE_IMPLIED, dex, 2},
	{"DEY", MODE_IMPLIED, dey, 2, 0, 0, 0, 0x88},
	{"DEZ", MODE_IMPLIED, dez, 2, 0, 0, 0, 0x3B},
	{"ASL", MODE_ABSOLUTE, asl_abs, 6, 0, 0, 0, 0x0E},
	{"ASL", MODE_IMPLIED, asla, 2, 0, 0, 0, 0x0A},
	{"ASL", MODE_ZP, asl_zp, 5, 0, 0, 0, 0x06},
	{"ASL", MODE_ZP_X, asl_zp_x, 6, 0, 0, 0, 0x16},
	{"LSR", MODE_ABSOLUTE, lsr_abs, 6, 0, 0, 0, 0x4E},
	{"LSR", MODE_IMPLIED, lsra, 2, 0, 0, 0, 0x4A},
	{"LSR", MODE_ZP, lsr_zp, 5, 0, 0, 0, 0x46},
	{"LSR", MODE_ZP_X, lsr_zp_x, 6, 0, 0, 0, 0x56},
	{"ROL", MODE_ABSOLUTE, rol_abs, 6, 0, 0, 0, 0x2E},
	{"ROL", MODE_IMPLIED, rola, 2, 0, 0, 0, 0x2A},
	{"ROL", MODE_ZP, rol_zp, 5, 0, 0, 0, 0x26},
	{"ROL", MODE_ZP_X, rol_zp_x, 6, 0, 0, 0, 0x36},
	{"ROR", MODE_ABSOLUTE, ror_abs, 6, 0, 0, 0, 0x6E},
	{"ROR", MODE_IMPLIED, rora, 2, 0, 0, 0, 0x6A},
	{"ROR", MODE_ZP, ror_zp, 5, 0, 0, 0, 0x66},
	{"ROR", MODE_ZP_X, ror_zp_x, 6, 0, 0, 0, 0x76},
	{"AND", MODE_IMMEDIATE, and_imm, 2, 0, 0, 0, 0x29},
	{"AND", MODE_ABSOLUTE, and_abs, 4, 0, 0, 0, 0x2D},
	{"AND", MODE_ZP, and_zp, 3, 0, 0, 0, 0x25},
	{"AND", MODE_ZP_X, and_zp_x, 4, 0, 0, 0, 0x35},
	{"EOR", MODE_IMMEDIATE, eor_imm, 2, 0, 0, 0, 0x49},
	{"EOR", MODE_ABSOLUTE, eor_abs, 4, 0, 0, 0, 0x4D},
	{"EOR", MODE_ZP, eor_zp, 3, 0, 0, 0, 0x45},
	{"EOR", MODE_ZP_X, eor_zp_x, 4, 0, 0, 0, 0x55},
	{"ORA", MODE_IMMEDIATE, ora_imm, 2, 0, 0, 0, 0x09},
	{"ORA", MODE_ABSOLUTE, ora_abs, 4, 0, 0, 0, 0x0D},
	{"ORA", MODE_ZP, ora_zp, 3, 0, 0, 0, 0x05},
	{"ORA", MODE_ZP_X, ora_zp_x, 4, 0, 0, 0, 0x15},
	{"BIT", MODE_ZP,       bit_zp,  3, 0, 0, 0, 0x24},
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
	{"BRA", MODE_RELATIVE, bra_rel, 2, 0, 0, 0, 0x80},
	{"BRA", MODE_RELATIVE_LONG, bra_relfar, 3, 0, 0, 0, 0x83},
	{"BRL", MODE_RELATIVE_LONG, bra_relfar, 3, 0, 0, 0, 0x83},
	{"LBRA", MODE_RELATIVE_LONG, bra_relfar, 3, 0, 0, 0, 0x83},
	{"JMP", MODE_ABSOLUTE, jmp_abs, 3, 0, 0, 0, 0x4C},
	{"JMP", MODE_INDIRECT_X, jmp_ind_x, 6, 0, 0, 0, 0x7C},
	{"JSR", MODE_ABSOLUTE, jsr_abs, 6, 0, 0, 0, 0x20},
	{"JSR", MODE_INDIRECT, jsr_ind, 5, 0, 0, 0, 0x22},
	{"JSR", MODE_ABS_INDIRECT_X, jsr_ind_x, 5, 0, 0, 0, 0x23},
	{"RTS", MODE_IMPLIED, rts, 6, 0, 0, 0, 0x60},
	{"BCC", MODE_RELATIVE, bcc, 2, 0, 0, 0, 0x90},
	{"BCC", MODE_RELATIVE_LONG, bcc_relfar, 3, 0, 0, 0, 0x93},
	{"LBCC", MODE_RELATIVE_LONG, bcc_relfar, 3, 0, 0, 0, 0x93},
	{"BCS", MODE_RELATIVE, bcs, 2, 0, 0, 0, 0xB0},
	{"BCS", MODE_RELATIVE_LONG, bcs_relfar, 3, 0, 0, 0, 0xB3},
	{"LBCS", MODE_RELATIVE_LONG, bcs_relfar, 3, 0, 0, 0, 0xB3},
	{"BEQ", MODE_RELATIVE, beq, 2, 0, 0, 0, 0xF0},
	{"BEQ", MODE_RELATIVE_LONG, beq_relfar, 3, 0, 0, 0, 0xF3},
	{"LBEQ", MODE_RELATIVE_LONG, beq_relfar, 3, 0, 0, 0, 0xF3},
	{"BNE", MODE_RELATIVE, bne, 2, 0, 0, 0, 0xD0},
	{"BNE", MODE_RELATIVE_LONG, bne_relfar, 3, 0, 0, 0, 0xD3},
	{"LBNE", MODE_RELATIVE_LONG, bne_relfar, 3, 0, 0, 0, 0xD3},
	{"BMI", MODE_RELATIVE, bmi, 2, 0, 0, 0, 0x30},
	{"BMI", MODE_RELATIVE_LONG, bmi_relfar, 3, 0, 0, 0, 0x33},
	{"LBMI", MODE_RELATIVE_LONG, bmi_relfar, 3, 0, 0, 0, 0x33},
	{"BPL", MODE_RELATIVE, bpl, 2, 0, 0, 0, 0x10},
	{"BPL", MODE_RELATIVE_LONG, bpl_relfar, 3, 0, 0, 0, 0x13},
	{"LBPL", MODE_RELATIVE_LONG, bpl_relfar, 3, 0, 0, 0, 0x13},
	{"BVS", MODE_RELATIVE, bvs, 2, 0, 0, 0, 0x70},
	{"BVS", MODE_RELATIVE_LONG, bvs_relfar, 3, 0, 0, 0, 0x73},
	{"LBVS", MODE_RELATIVE_LONG, bvs_relfar, 3, 0, 0, 0, 0x73},
	{"BVC", MODE_RELATIVE, bvc, 2, 0, 0, 0, 0x50},
	{"BVC", MODE_RELATIVE_LONG, bvc_relfar, 3, 0, 0, 0, 0x53},
	{"LBVC", MODE_RELATIVE_LONG, bvc_relfar, 3, 0, 0, 0, 0x53},
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
	{"TZX", MODE_IMPLIED, tzx, 2},
	{"TXZ", MODE_IMPLIED, txz, 2},
	{"TZY", MODE_IMPLIED, tzy, 2},
	{"TYZ", MODE_IMPLIED, tyz, 2},
	{"PHA", MODE_IMPLIED, pha, 3, 0, 0, 0, 0x48},
	{"PLA", MODE_IMPLIED, pla, 4, 0, 0, 0, 0x68},
	{"PHX", MODE_IMPLIED, phx, 3, 0, 0, 0, 0xDA},
	{"PLX", MODE_IMPLIED, plx, 4, 0, 0, 0, 0xFA},
	{"PHY", MODE_IMPLIED, phy, 3, 0, 0, 0, 0x5A},
	{"PLY", MODE_IMPLIED, ply, 4, 0, 0, 0, 0x7A},
	{"PHP", MODE_IMPLIED, php_45gs02, 3, 0, 0, 0, 0x08},
	{"PLP", MODE_IMPLIED, plp, 4, 0, 0, 0, 0x28},
	{"BRK", MODE_IMPLIED, brk, 7, 0, 0, 0, 0x00},
	{"RTI", MODE_IMPLIED, rti, 6, 0, 0, 0, 0x40},
	{"WAI", MODE_IMPLIED, wai, 3},
	{"STP", MODE_IMPLIED, stp, 3},
	{"EOM", MODE_IMPLIED, eom, 2},
	{"NOP", MODE_IMPLIED, nop, 2},
	/* Quad (32-bit) instructions - NEG NEG ($42 $42) prefix */
	{"LDQ",  MODE_ZP,            ldq_zp,        5},
	{"LDQ",  MODE_ABSOLUTE,      ldq_abs,       5},
	{"LDQ",  MODE_ZP_INDIRECT,   ldq_zp_ind,    9},
	{"LDQ",  MODE_ZP_INDIRECT_Z, ldq_zp_ind_z,  9},
	{"STQ",  MODE_ZP,            stq_zp,        5},
	{"STQ",  MODE_ABSOLUTE,      stq_abs,       5},
	{"STQ",  MODE_ZP_INDIRECT,   stq_zp_ind,    9},
	{"STQ",  MODE_ZP_INDIRECT_Z, stq_zp_ind_z,  9},
	{"ADCQ", MODE_ZP,       adcq_zp,  5},
	{"ADCQ", MODE_ABSOLUTE, adcq_abs, 5},
	{"SBCQ", MODE_ZP,       sbcq_zp,  5},
	{"SBCQ", MODE_ABSOLUTE, sbcq_abs, 5},
	{"CPQ",  MODE_ZP,       cpq_zp,   5},
	{"CPQ",  MODE_ABSOLUTE, cpq_abs,  5},
	{"EORQ", MODE_ZP,       eorq_zp,  5},
	{"EORQ", MODE_ABSOLUTE, eorq_abs, 5},
	{"ANDQ", MODE_ZP,       andq_zp,  5},
	{"ANDQ", MODE_ABSOLUTE, andq_abs, 5},
	{"BITQ", MODE_ZP,       bitq_zp,  5},
	{"BITQ", MODE_ABSOLUTE, bitq_abs, 5},
	{"ORQ",  MODE_ZP,       orq_zp,   5},
	{"ORQ",  MODE_ABSOLUTE, orq_abs,  5},
	{"ASLQ", MODE_IMPLIED,  aslq,    2},
	{"ASRQ", MODE_IMPLIED,  asrq,    2},
	{"LSRQ", MODE_IMPLIED,  lsrq,    2},
	{"ROLQ", MODE_IMPLIED,  rolq,    2},
	{"RORQ", MODE_IMPLIED,  rorq,    2},
	{"INQ",  MODE_IMPLIED,  inq,     2},
	{"DEQ",  MODE_IMPLIED,  deq,     2},
};

int OPCODES_45GS02_COUNT = sizeof(opcodes_45gs02) / sizeof(opcodes_45gs02[0]);
