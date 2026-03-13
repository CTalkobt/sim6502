#include "opcodes.h"

/* 45GS02 16-bit stack helpers.
 * E=1 (emulation): 8-bit SP, stack on page 1 (0x0100-0x01FF), wraps at 0xFF.
 * E=0 (extended):  16-bit SP, stack can be anywhere in 64KB. */
static inline unsigned short stack45_addr(cpu_t *cpu) {
	return cpu->get_flag(FLAG_E) ? (0x0100 | (cpu->s & 0xFF)) : cpu->s;
}

static inline void stack45_push(cpu_t *cpu) {
	if (cpu->get_flag(FLAG_E))
		cpu->s = (cpu->s & 0xFF00) | ((cpu->s - 1) & 0xFF);
	else
		cpu->s--;
}

static inline void stack45_pop(cpu_t *cpu) {
	if (cpu->get_flag(FLAG_E))
		cpu->s = (cpu->s & 0xFF00) | ((cpu->s + 1) & 0xFF);
	else
		cpu->s++;
}

static void ldz_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = arg & 0xFF;
	cpu->update_nz(cpu->z);
	cpu->cycles += 2;
	cpu->pc += 2;
}

static void ldz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = mem_read(mem, arg);
	cpu->update_nz(cpu->z);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void inz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z++;
	cpu->update_nz(cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void dez(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z--;
	cpu->update_nz(cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void cpz_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = arg & 0xFF;
	int result = cpu->z - val;
	cpu->set_flag(FLAG_C, cpu->z >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 2;
	cpu->pc += 2;
}

static void cpz_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	int result = cpu->z - val;
	cpu->set_flag(FLAG_C, cpu->z >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void cpz_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->z - val;
	cpu->set_flag(FLAG_C, cpu->z >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void tsy(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->y = (unsigned char)cpu->s;  /* low byte of 16-bit SP */
	cpu->update_nz(cpu->y);
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
	cpu->update_nz(cpu->z);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tza(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->z;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void cle(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_E, 0);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void see(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_E, 1);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void neg(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = (unsigned char)((-cpu->a) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asr_acc(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->set_flag(FLAG_C, cpu->a & 0x01);
	cpu->a = (unsigned char)((cpu->a >> 1) | (cpu->a & 0x80));
	cpu->update_nz(cpu->a);
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
	cpu->set_flag(FLAG_C, val & 0x01);
	val = (unsigned char)((val >> 1) | (val & 0x80));
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void asr_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = (unsigned char)((val >> 1) | (val & 0x80));
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void tab(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->b = cpu->a;
	cpu->update_nz(cpu->b);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void tba(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a = cpu->b;
	cpu->update_nz(cpu->a);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asw(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	cpu->set_flag(FLAG_C, val & 0x8000);
	val = (val << 1) & 0xFFFF;
	mem_write(mem, arg, val & 0xFF);
	mem_write(mem, arg + 1, (val >> 8) & 0xFF);
	cpu->update_nz(val & 0xFF); /* NZ updated based on low byte? Or 16-bit? Book says 8-bit usually */
	cpu->cycles += 6;
	cpu->pc += 3;
}

static void row(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	int old_c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = (val >> 1) | (old_c << 15);
	mem_write(mem, arg, val & 0xFF);
	mem_write(mem, arg + 1, (val >> 8) & 0xFF);
	cpu->update_nz(val & 0xFF);
	cpu->cycles += 6;
	cpu->pc += 3;
}

static void dew_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	val--;
	mem_write(mem, arg & 0xFF, val & 0xFF);
	mem_write(mem, (arg + 1) & 0xFF, (val >> 8) & 0xFF);
	cpu->set_flag(FLAG_Z, val == 0);
	cpu->set_flag(FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void inw_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	val++;
	mem_write(mem, arg & 0xFF, val & 0xFF);
	mem_write(mem, (arg + 1) & 0xFF, (val >> 8) & 0xFF);
	cpu->set_flag(FLAG_Z, val == 0);
	cpu->set_flag(FLAG_N, val & 0x8000);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void phw_imm(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, stack45_addr(cpu), (arg >> 8) & 0xFF);
	stack45_push(cpu);
	mem_write(mem, stack45_addr(cpu), arg & 0xFF);
	stack45_push(cpu);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void phw_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short val = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	mem_write(mem, stack45_addr(cpu), (val >> 8) & 0xFF);
	stack45_push(cpu);
	mem_write(mem, stack45_addr(cpu), val & 0xFF);
	stack45_push(cpu);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void phz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, stack45_addr(cpu), cpu->z);
	stack45_push(cpu);
	cpu->cycles += 3;
	cpu->pc += 1;
}

static void plz(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	cpu->z = mem_read(mem, stack45_addr(cpu));
	cpu->update_nz(cpu->z);
	cpu->cycles += 4;
	cpu->pc += 1;
}

static void ldz_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->z = mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->z);
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
	cpu->update_nz(cpu->a);
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
	cpu->do_adc(val);
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
	cpu->do_sbc(val);
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
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
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
	cpu->update_nz(cpu->a);
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
	cpu->update_nz(cpu->a);
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
	cpu->update_nz(cpu->a);
	cpu->pc += 2;
}

static void lda_sp_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ptr_addr = (stack45_addr(cpu) + (arg & 0xFF)) & 0xFFFF;
	unsigned short addr = mem_read(mem, ptr_addr) | (mem_read(mem, (ptr_addr + 1) & 0xFFFF) << 8);
	cpu->a = mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void sta_sp_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ptr_addr = (stack45_addr(cpu) + (arg & 0xFF)) & 0xFFFF;
	unsigned short addr = mem_read(mem, ptr_addr) | (mem_read(mem, (ptr_addr + 1) & 0xFFFF) << 8);
	mem_write(mem, addr + cpu->y, cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void jsr_ind(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, stack45_addr(cpu), (ret >> 8) & 0xFF);
	stack45_push(cpu);
	mem_write(mem, stack45_addr(cpu), ret & 0xFF);
	stack45_push(cpu);
	unsigned short addr = mem_read(mem, arg) | (mem_read(mem, arg + 1) << 8);
	cpu->pc = addr;
	cpu->cycles += 5;
}

static void jsr_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, stack45_addr(cpu), (ret >> 8) & 0xFF);
	stack45_push(cpu);
	mem_write(mem, stack45_addr(cpu), ret & 0xFF);
	stack45_push(cpu);
	unsigned short addr = mem_read(mem, arg + cpu->x) | (mem_read(mem, arg + cpu->x + 1) << 8);
	cpu->pc = addr;
	cpu->cycles += 5;
}

static void rtn(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	unsigned short lo = mem_read(mem, stack45_addr(cpu));
	stack45_pop(cpu);
	unsigned short hi = mem_read(mem, stack45_addr(cpu));
	/* Discard stack-frame parameters, respecting stack mode */
	unsigned char n = arg & 0xFF;
	if (cpu->get_flag(FLAG_E))
		cpu->s = (cpu->s & 0xFF00) | ((cpu->s + n) & 0xFF);
	else
		cpu->s += n;
	cpu->pc = (hi << 8) | lo;
	cpu->pc++; /* return address is stored as addr-1, same as RTS */
	cpu->cycles += 6;
}

static void bra_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* 16-bit relative branch; offset is relative to last operand byte (KickAssembler convention) */
	cpu->pc += (short)arg + 2;
	cpu->cycles += 4;
}

static void bsr_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	/* Push return address (last byte of BSR instruction) */
	unsigned short ret = cpu->pc + 2;
	mem_write(mem, stack45_addr(cpu), (ret >> 8) & 0xFF);
	stack45_push(cpu);
	mem_write(mem, stack45_addr(cpu), ret & 0xFF);
	stack45_push(cpu);
	/* 16-bit relative branch; offset is relative to last operand byte */
	cpu->pc += (short)arg + 2;
	cpu->cycles += 4;
}

static void bcc_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_C)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bcs_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_C)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void beq_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_Z)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bne_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_Z)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bmi_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_N)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bpl_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_N)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bvc_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (!cpu->get_flag(FLAG_V)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void bvs_relfar(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	if (cpu->get_flag(FLAG_V)) {
		cpu->pc += (short)arg + 2;
		cpu->cycles += 4;
	} else {
		cpu->pc += 3;
		cpu->cycles += 3;
	}
}

static void asl_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void asl_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void lsr_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void lsr_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void rol_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void rol_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void ror_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void ror_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void inc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) + 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void inc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, (arg + cpu->x) & 0xFF) + 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void dec_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg & 0xFF) - 1) & 0xFF;
	mem_write(mem, arg & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void dec_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, (arg + cpu->x) & 0xFF) - 1) & 0xFF;
	mem_write(mem, (arg + cpu->x) & 0xFF, val);
	cpu->update_nz(val);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void adc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->do_adc(val);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void adc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->do_adc(val);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void sbc_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	cpu->do_sbc(val);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void sbc_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->do_sbc(val);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void and_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void and_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void eor_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void eor_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void ora_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void ora_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, (arg + cpu->x) & 0xFF);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void cmp_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void cmp_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, (arg + cpu->x) & 0xFF);
	int result = cpu->a - val;
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 2;
}

static void cpx_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->x - val;
	cpu->set_flag(FLAG_C, cpu->x >= val);
	cpu->update_nz(result & 0xFF);
	cpu->cycles += 3;
	cpu->pc += 2;
}

static void cpy_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg & 0xFF);
	int result = cpu->y - val;
	cpu->set_flag(FLAG_C, cpu->y >= val);
	cpu->update_nz(result & 0xFF);
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

/* PHP on 45GS02: bit 5 is the E flag (not the unused/always-1 bit), so we
 * do not force it to 1.  Stack address respects the current E flag mode. */
static void php_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, stack45_addr(cpu), cpu->p | FLAG_B);
	stack45_push(cpu);
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
	cpu->set_flag(FLAG_Z, val == 0);
	cpu->set_flag(FLAG_N, (val >> 31) & 1);
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
	unsigned long long result = (unsigned long long)q + val + cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, result > 0xFFFFFFFFUL);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	cpu->set_flag(FLAG_V, (~(q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void adcq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	unsigned long long result = (unsigned long long)q + val + cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, result > 0xFFFFFFFFUL);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	cpu->set_flag(FLAG_V, (~(q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void sbcq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	long long result = (long long)q - (long long)val - (1 - cpu->get_flag(FLAG_C));
	cpu->set_flag(FLAG_C, result >= 0);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	cpu->set_flag(FLAG_V, ((q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void sbcq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	long long result = (long long)q - (long long)val - (1 - cpu->get_flag(FLAG_C));
	cpu->set_flag(FLAG_C, result >= 0);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	cpu->set_flag(FLAG_V, ((q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void cpq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	cpu->set_flag(FLAG_C, q >= val);
	update_nz_q(cpu, q - val);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void cpq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	cpu->set_flag(FLAG_C, q >= val);
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
	cpu->set_flag(FLAG_Z, (q & val) == 0);
	cpu->set_flag(FLAG_N, (val >> 31) & 1);
	cpu->set_flag(FLAG_V, (val >> 30) & 1);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void bitq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	unsigned int val = mem_read32_abs(mem, arg);
	cpu->set_flag(FLAG_Z, (q & val) == 0);
	cpu->set_flag(FLAG_N, (val >> 31) & 1);
	cpu->set_flag(FLAG_V, (val >> 30) & 1);
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
	cpu->set_flag(FLAG_C, (q >> 31) & 1);
	q <<= 1;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void asrq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	cpu->set_flag(FLAG_C, q & 1);
	q = (q >> 1) | (q & 0x80000000); /* preserve sign bit */
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void lsrq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	cpu->set_flag(FLAG_C, q & 1);
	q >>= 1;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void rolq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, (q >> 31) & 1);
	q = (q << 1) | (unsigned int)c;
	set_q(cpu, q);
	update_nz_q(cpu, q);
	cpu->cycles += 2;
	cpu->pc += 1;
}

static void rorq(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int q = get_q(cpu);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, q & 1);
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

/* --- Missing standard opcodes --- */

static void ora_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a |= mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void ora_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a |= mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void ora_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void ora_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a |= mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void and_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a &= mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void and_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a &= mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void and_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void and_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a &= mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void eor_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->a ^= mem_read(mem, addr);
	cpu->update_nz(cpu->a);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void eor_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->a ^= mem_read(mem, addr + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void eor_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg + cpu->y);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void eor_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->a ^= mem_read(mem, arg + cpu->x);
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void sbc_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	cpu->do_sbc(mem_read(mem, addr));
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void sbc_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	cpu->do_sbc(mem_read(mem, addr + cpu->y));
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void sbc_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->do_sbc(mem_read(mem, arg + cpu->y));
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void sbc_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->do_sbc(mem_read(mem, arg + cpu->x));
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void cmp_ind_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, (arg + cpu->x) & 0xFF) |
		(mem_read(mem, (arg + cpu->x + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 6;
	cpu->pc += 2;
}

static void cmp_ind_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short addr = mem_read(mem, arg & 0xFF) | (mem_read(mem, (arg + 1) & 0xFF) << 8);
	unsigned char val = mem_read(mem, addr + cpu->y);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 5;
	cpu->pc += 2;
}

static void cmp_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->y);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void cmp_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	cpu->set_flag(FLAG_C, cpu->a >= val);
	cpu->update_nz((cpu->a - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void asl_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = (val << 1) & 0xFF;
	mem_write(mem, arg + cpu->x, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

static void rol_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x80);
	val = ((val << 1) | c) & 0xFF;
	mem_write(mem, arg + cpu->x, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

static void lsr_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	cpu->set_flag(FLAG_C, val & 0x01);
	val >>= 1;
	mem_write(mem, arg + cpu->x, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

static void ror_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg + cpu->x);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 0x01);
	val = ((val >> 1) | (c << 7)) & 0xFF;
	mem_write(mem, arg + cpu->x, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

static void sty_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->x, cpu->y);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void stx_abs_y(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, arg + cpu->y, cpu->x);
	cpu->cycles += 5;
	cpu->pc += 3;
}

static void cpx_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, cpu->x >= val);
	cpu->update_nz((cpu->x - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void cpy_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = mem_read(mem, arg);
	cpu->set_flag(FLAG_C, cpu->y >= val);
	cpu->update_nz((cpu->y - val) & 0xFF);
	cpu->cycles += 4;
	cpu->pc += 3;
}

static void dec_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg + cpu->x) - 1) & 0xFF;
	mem_write(mem, arg + cpu->x, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

static void inc_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char val = (mem_read(mem, arg + cpu->x) + 1) & 0xFF;
	mem_write(mem, arg + cpu->x, val);
	cpu->update_nz(val);
	cpu->cycles += 7;
	cpu->pc += 3;
}

/* --- Quad memory-addressed shift/rotate/inc/dec --- */

static void aslq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val <<= 1;
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void aslq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val <<= 1;
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void aslq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val <<= 1;
	mem_write32_abs(mem, arg, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 3;
}

static void aslq_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ea = (unsigned short)(arg + cpu->x);
	unsigned int val = mem_read32_abs(mem, ea);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val <<= 1;
	mem_write32_abs(mem, ea, val);
	update_nz_q(cpu, val);
	cpu->cycles += 9;
	cpu->pc += 3;
}

static void asrq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	cpu->set_flag(FLAG_C, val & 1);
	val = (val >> 1) | (val & 0x80000000);
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void asrq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr);
	cpu->set_flag(FLAG_C, val & 1);
	val = (val >> 1) | (val & 0x80000000);
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void lsrq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	cpu->set_flag(FLAG_C, val & 1);
	val >>= 1;
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void lsrq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr);
	cpu->set_flag(FLAG_C, val & 1);
	val >>= 1;
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void lsrq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg);
	cpu->set_flag(FLAG_C, val & 1);
	val >>= 1;
	mem_write32_abs(mem, arg, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 3;
}

static void lsrq_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ea = (unsigned short)(arg + cpu->x);
	unsigned int val = mem_read32_abs(mem, ea);
	cpu->set_flag(FLAG_C, val & 1);
	val >>= 1;
	mem_write32_abs(mem, ea, val);
	update_nz_q(cpu, val);
	cpu->cycles += 9;
	cpu->pc += 3;
}

static void rolq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val = (val << 1) | (unsigned int)c;
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void rolq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val = (val << 1) | (unsigned int)c;
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void rolq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val = (val << 1) | (unsigned int)c;
	mem_write32_abs(mem, arg, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 3;
}

static void rolq_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ea = (unsigned short)(arg + cpu->x);
	unsigned int val = mem_read32_abs(mem, ea);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, (val >> 31) & 1);
	val = (val << 1) | (unsigned int)c;
	mem_write32_abs(mem, ea, val);
	update_nz_q(cpu, val);
	cpu->cycles += 9;
	cpu->pc += 3;
}

static void rorq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF));
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 1);
	val = (val >> 1) | ((unsigned int)c << 31);
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void rorq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 1);
	val = (val >> 1) | ((unsigned int)c << 31);
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void rorq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 1);
	val = (val >> 1) | ((unsigned int)c << 31);
	mem_write32_abs(mem, arg, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 3;
}

static void rorq_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ea = (unsigned short)(arg + cpu->x);
	unsigned int val = mem_read32_abs(mem, ea);
	int c = cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, val & 1);
	val = (val >> 1) | ((unsigned int)c << 31);
	mem_write32_abs(mem, ea, val);
	update_nz_q(cpu, val);
	cpu->cycles += 9;
	cpu->pc += 3;
}

static void inq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF)) + 1;
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void inq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr) + 1;
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void inq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg) + 1;
	mem_write32_abs(mem, arg, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 3;
}

static void inq_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ea = (unsigned short)(arg + cpu->x);
	unsigned int val = mem_read32_abs(mem, ea) + 1;
	mem_write32_abs(mem, ea, val);
	update_nz_q(cpu, val);
	cpu->cycles += 9;
	cpu->pc += 3;
}

static void deq_zp(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_zp(mem, (unsigned char)(arg & 0xFF)) - 1;
	mem_write32_zp(mem, (unsigned char)(arg & 0xFF), val);
	update_nz_q(cpu, val);
	cpu->cycles += 7;
	cpu->pc += 2;
}

static void deq_zp_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char addr = (unsigned char)((arg + cpu->x) & 0xFF);
	unsigned int val = mem_read32_zp(mem, addr) - 1;
	mem_write32_zp(mem, addr, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 2;
}

static void deq_abs(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned int val = mem_read32_abs(mem, arg) - 1;
	mem_write32_abs(mem, arg, val);
	update_nz_q(cpu, val);
	cpu->cycles += 8;
	cpu->pc += 3;
}

static void deq_abs_x(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned short ea = (unsigned short)(arg + cpu->x);
	unsigned int val = mem_read32_abs(mem, ea) - 1;
	mem_write32_abs(mem, ea, val);
	update_nz_q(cpu, val);
	cpu->cycles += 9;
	cpu->pc += 3;
}

/* --- Quad indirect-Z ALU variants --- */

static void adcq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		cpu->cycles += 11;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		cpu->cycles += 9;
	}
	unsigned int q = get_q(cpu);
	unsigned long long result = (unsigned long long)q + val + cpu->get_flag(FLAG_C);
	cpu->set_flag(FLAG_C, result > 0xFFFFFFFFUL);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	cpu->set_flag(FLAG_V, (~(q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->pc += 2;
}

static void sbcq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		cpu->cycles += 11;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		cpu->cycles += 9;
	}
	unsigned int q = get_q(cpu);
	long long result = (long long)q - (long long)val - (1 - cpu->get_flag(FLAG_C));
	cpu->set_flag(FLAG_C, result >= 0);
	unsigned int r = (unsigned int)(result & 0xFFFFFFFF);
	cpu->set_flag(FLAG_V, ((q ^ val) & (q ^ r) & 0x80000000) != 0);
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->pc += 2;
}

static void cmpq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		cpu->cycles += 11;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		cpu->cycles += 9;
	}
	unsigned int q = get_q(cpu);
	cpu->set_flag(FLAG_C, q >= val);
	update_nz_q(cpu, q - val);
	cpu->pc += 2;
}

static void andq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		cpu->cycles += 11;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		cpu->cycles += 9;
	}
	unsigned int r = get_q(cpu) & val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->pc += 2;
}

static void eorq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		cpu->cycles += 11;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		cpu->cycles += 9;
	}
	unsigned int r = get_q(cpu) ^ val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->pc += 2;
}

static void orq_zp_ind_z(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	unsigned char zp = (unsigned char)(arg & 0xFF);
	unsigned int val;
	if (cpu->eom_prefix) {
		cpu->eom_prefix = 0;
		unsigned int addr = (unsigned int)mem_read(mem, zp)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 1)) << 8)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 2)) << 16)
			| ((unsigned int)mem_read(mem, (unsigned char)(zp + 3)) << 24);
		addr += cpu->z;
		val = (unsigned int)far_mem_read(mem, addr)
			| ((unsigned int)far_mem_read(mem, addr + 1) << 8)
			| ((unsigned int)far_mem_read(mem, addr + 2) << 16)
			| ((unsigned int)far_mem_read(mem, addr + 3) << 24);
		cpu->cycles += 11;
	} else {
		unsigned short ptr = mem_read(mem, zp) | (mem_read(mem, (unsigned char)(zp + 1)) << 8);
		val = mem_read32_abs(mem, (unsigned short)(ptr + cpu->z));
		cpu->cycles += 9;
	}
	unsigned int r = get_q(cpu) | val;
	set_q(cpu, r);
	update_nz_q(cpu, r);
	cpu->pc += 2;
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
extern void jmp_abs_indirect(cpu_t *cpu, memory_t *mem, unsigned short arg);
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
extern void eom(cpu_t *cpu, memory_t *mem, unsigned short arg);
extern void op_nop(cpu_t *cpu, memory_t *mem, unsigned short arg);

/* 45GS02 E-flag-aware stack operations — replace shared 6502 versions. */
static void pha_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, stack45_addr(cpu), cpu->a);
	stack45_push(cpu);
	cpu->cycles += 3;
	cpu->pc += 1;
}

static void pla_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	cpu->a = mem_read(mem, stack45_addr(cpu));
	cpu->update_nz(cpu->a);
	cpu->cycles += 4;
	cpu->pc += 1;
}

static void plp_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	cpu->p = mem_read(mem, stack45_addr(cpu));
	cpu->cycles += 4;
	cpu->pc += 1;
}

static void phx_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, stack45_addr(cpu), cpu->x);
	stack45_push(cpu);
	cpu->cycles += 3;
	cpu->pc += 1;
}

static void plx_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	cpu->x = mem_read(mem, stack45_addr(cpu));
	cpu->update_nz(cpu->x);
	cpu->cycles += 4;
	cpu->pc += 1;
}

static void phy_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	mem_write(mem, stack45_addr(cpu), cpu->y);
	stack45_push(cpu);
	cpu->cycles += 3;
	cpu->pc += 1;
}

static void ply_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	cpu->y = mem_read(mem, stack45_addr(cpu));
	cpu->update_nz(cpu->y);
	cpu->cycles += 4;
	cpu->pc += 1;
}

static void brk_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	cpu->cycles += 7;
	cpu->pc += 2;
	mem_write(mem, stack45_addr(cpu), (cpu->pc >> 8) & 0xFF);
	stack45_push(cpu);
	mem_write(mem, stack45_addr(cpu), cpu->pc & 0xFF);
	stack45_push(cpu);
	cpu->set_flag(FLAG_B, 1);
	mem_write(mem, stack45_addr(cpu), cpu->p);
	stack45_push(cpu);
	cpu->set_flag(FLAG_I, 1);
}

static void rti_45gs02(cpu_t *cpu, memory_t *mem, unsigned short arg) {
	stack45_pop(cpu);
	cpu->p = mem_read(mem, stack45_addr(cpu));
	stack45_pop(cpu);
	unsigned short ret = mem_read(mem, stack45_addr(cpu));
	stack45_pop(cpu);
	ret |= mem_read(mem, stack45_addr(cpu)) << 8;
	cpu->pc = ret;
}

opcode_handler_t opcodes_45gs02[] = {
	{"LDA", MODE_IMMEDIATE, lda_imm, 2, 0, 0, 0, {0xA9}, 1},
	{"LDA", MODE_ABSOLUTE, lda_abs, 4, 0, 0, 0, {0xAD}, 1},
	{"LDA", MODE_ABSOLUTE_X, lda_abs_x, 4, 0, 0, 0, {0xBD}, 1},
	{"LDA", MODE_ABSOLUTE_Y, lda_abs_y, 4, 0, 0, 0, {0xB9}, 1},
	{"LDA", MODE_ZP, lda_zp, 3, 0, 0, 0, {0xA5}, 1},
	{"LDA", MODE_ZP_X, lda_zp_x, 4, 0, 0, 0, {0xB5}, 1},
	{"ASW", MODE_ABSOLUTE, asw, 6, 0, 0, 0, {0xCB}, 1},
	{"ROW", MODE_ABSOLUTE, row, 6, 0, 0, 0, {0xEB}, 1},
	{"DEW", MODE_ZP, dew_zp, 6, 0, 0, 0, {0xC3}, 1},
	{"INW", MODE_ZP, inw_zp, 6, 0, 0, 0, {0xE3}, 1},
	{"PHW", MODE_IMMEDIATE_WORD, phw_imm, 4, 0, 0, 0, {0xF4}, 1},
	{"PHW", MODE_ABSOLUTE, phw_abs, 5, 0, 0, 0, {0xFC}, 1},
	{"PHZ", MODE_IMPLIED, phz, 3, 0, 0, 0, {0xDB}, 1},
	{"PLZ", MODE_IMPLIED, plz, 4, 0, 0, 0, {0xFB}, 1},
	{"LDZ", MODE_ABSOLUTE_X, ldz_abs_x, 4, 0, 0, 0, {0xBB}, 1},
	{"TSY", MODE_IMPLIED, tsy, 2, 0, 0, 0, {0x0B}, 1},
	{"TYS", MODE_IMPLIED, tys, 2, 0, 0, 0, {0x2B}, 1},
	{"TAZ", MODE_IMPLIED, taz, 2, 0, 0, 0, {0x4B}, 1},
	{"TZA", MODE_IMPLIED, tza, 2, 0, 0, 0, {0x6B}, 1},
	{"CLE", MODE_IMPLIED, cle, 2, 0, 0, 0, {0x02}, 1},
	{"SEE", MODE_IMPLIED, see, 2, 0, 0, 0, {0x03}, 1},
	{"NEG", MODE_IMPLIED, neg, 2, 0, 0, 0, {0x42}, 1},
	{"TAB", MODE_IMPLIED, tab, 2, 0, 0, 0, {0x5B}, 1},
	{"TBA", MODE_IMPLIED, tba, 2, 0, 0, 0, {0x7B}, 1},
	{"RTN", MODE_IMMEDIATE, rtn, 6, 0, 0, 0, {0x62}, 1},
	{"BSR", MODE_RELATIVE_LONG, bsr_relfar, 4, 0, 0, 0, {0x63}, 1},
	{"LBSR", MODE_RELATIVE_LONG, bsr_relfar, 4, 0, 0, 0, {0x63}, 1},
	{"ASR", MODE_IMPLIED, asr_acc, 2, 0, 0, 0, {0x43}, 1},
	{"ASR", MODE_ZP, asr_zp, 5, 0, 0, 0, {0x44}, 1},
	{"ASR", MODE_ZP_X, asr_zp_x, 6, 0, 0, 0, {0x54}, 1},
	{"MAP", MODE_IMPLIED, map, 2, 0, 0, 0, {0x5C}, 1},
	{"LDA", MODE_INDIRECT_X, lda_ind_x, 6, 0, 0, 0, {0xA1}, 1},
	{"LDA", MODE_INDIRECT_Y, lda_ind_y, 5, 0, 0, 0, {0xB1}, 1},
	{"LDZ", MODE_IMMEDIATE, ldz_imm, 2, 0, 0, 0, {0xA3}, 1},
	{"LDZ", MODE_ABSOLUTE, ldz_abs, 4, 0, 0, 0, {0xAB}, 1},
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
	{"STZ", MODE_ABSOLUTE, stz_abs, 4, 0, 0, 0, {0x9C}, 1},
	{"STZ", MODE_ZP, stz_zp, 3, 0, 0, 0, {0x64}, 1},
	{"STX", MODE_ABSOLUTE, stx_abs, 4, 0, 0, 0, {0x8E}, 1},
	{"STX", MODE_ZP, stx_zp, 3, 0, 0, 0, {0x86}, 1},
	{"STX", MODE_ZP_Y, stx_zp_y, 4, 0, 0, 0, {0x96}, 1},
	{"STX", MODE_ABSOLUTE_Y, stx_abs_y, 5, 0, 0, 0, {0x9B}, 1},
	{"STY", MODE_ABSOLUTE, sty_abs, 4, 0, 0, 0, {0x8C}, 1},
	{"STY", MODE_ZP, sty_zp, 3, 0, 0, 0, {0x84}, 1},
	{"STY", MODE_ZP_X, sty_zp_x, 4, 0, 0, 0, {0x94}, 1},
	{"STY", MODE_ABSOLUTE_X, sty_abs_x, 5, 0, 0, 0, {0x8B}, 1},
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
	{"SBC", MODE_ZP, sbc_zp, 3, 0, 0, 0, {0xE5}, 1},
	{"SBC", MODE_ZP_X, sbc_zp_x, 4, 0, 0, 0, {0xF5}, 1},
	{"SBC", MODE_INDIRECT_X, sbc_ind_x, 6, 0, 0, 0, {0xE1}, 1},
	{"SBC", MODE_INDIRECT_Y, sbc_ind_y, 5, 0, 0, 0, {0xF1}, 1},
	{"SBC", MODE_ABSOLUTE_Y, sbc_abs_y, 4, 0, 0, 0, {0xF9}, 1},
	{"SBC", MODE_ABSOLUTE_X, sbc_abs_x, 4, 0, 0, 0, {0xFD}, 1},
	{"LDA", MODE_ZP_INDIRECT_Z, lda_zp_ind_z, 5, 0, 0, 0, {0xB2}, 1},
	{"LDA", MODE_SP_INDIRECT_Y, lda_sp_ind_y, 6, 0, 0, 0, {0xE2}, 1},
	{"STA", MODE_ZP_INDIRECT_Z, sta_zp_ind_z, 5, 0, 0, 0, {0x92}, 1},
	{"STA", MODE_SP_INDIRECT_Y, sta_sp_ind_y, 6, 0, 0, 0, {0x82}, 1},
	{"ADC", MODE_ZP_INDIRECT_Z, adc_zp_ind_z, 5, 0, 0, 0, {0x72}, 1},
	{"SBC", MODE_ZP_INDIRECT_Z, sbc_zp_ind_z, 5, 0, 0, 0, {0xF2}, 1},
	{"CMP", MODE_ZP_INDIRECT_Z, cmp_zp_ind_z, 5, 0, 0, 0, {0xD2}, 1},
	{"ORA", MODE_ZP_INDIRECT_Z, ora_zp_ind_z, 5, 0, 0, 0, {0x12}, 1},
	{"AND", MODE_ZP_INDIRECT_Z, and_zp_ind_z, 5, 0, 0, 0, {0x32}, 1},
	{"EOR", MODE_ZP_INDIRECT_Z, eor_zp_ind_z, 5, 0, 0, 0, {0x52}, 1},
	/* Flat [zp],Z variants — EOM ($EA) prefix signals 32-bit pointer */
	{"LDA", MODE_ZP_INDIRECT_Z_FLAT, lda_zp_ind_z, 8, 0, 0, 0, {0xEA, 0xB2}, 2},
	{"STA", MODE_ZP_INDIRECT_Z_FLAT, sta_zp_ind_z, 8, 0, 0, 0, {0xEA, 0x92}, 2},
	{"ADC", MODE_ZP_INDIRECT_Z_FLAT, adc_zp_ind_z, 8, 0, 0, 0, {0xEA, 0x72}, 2},
	{"SBC", MODE_ZP_INDIRECT_Z_FLAT, sbc_zp_ind_z, 8, 0, 0, 0, {0xEA, 0xF2}, 2},
	{"CMP", MODE_ZP_INDIRECT_Z_FLAT, cmp_zp_ind_z, 8, 0, 0, 0, {0xEA, 0xD2}, 2},
	{"ORA", MODE_ZP_INDIRECT_Z_FLAT, ora_zp_ind_z, 8, 0, 0, 0, {0xEA, 0x12}, 2},
	{"AND", MODE_ZP_INDIRECT_Z_FLAT, and_zp_ind_z, 8, 0, 0, 0, {0xEA, 0x32}, 2},
	{"EOR", MODE_ZP_INDIRECT_Z_FLAT, eor_zp_ind_z, 8, 0, 0, 0, {0xEA, 0x52}, 2},
	{"CMP", MODE_IMMEDIATE, cmp_imm, 2, 0, 0, 0, {0xC9}, 1},
	{"CMP", MODE_ABSOLUTE, cmp_abs, 4, 0, 0, 0, {0xCD}, 1},
	{"CMP", MODE_ZP, cmp_zp, 3, 0, 0, 0, {0xC5}, 1},
	{"CMP", MODE_ZP_X, cmp_zp_x, 4, 0, 0, 0, {0xD5}, 1},
	{"CMP", MODE_INDIRECT_X, cmp_ind_x, 6, 0, 0, 0, {0xC1}, 1},
	{"CMP", MODE_INDIRECT_Y, cmp_ind_y, 5, 0, 0, 0, {0xD1}, 1},
	{"CMP", MODE_ABSOLUTE_Y, cmp_abs_y, 4, 0, 0, 0, {0xD9}, 1},
	{"CMP", MODE_ABSOLUTE_X, cmp_abs_x, 4, 0, 0, 0, {0xDD}, 1},
	{"CPZ", MODE_IMMEDIATE, cpz_imm, 2, 0, 0, 0, {0xC2}, 1},
	{"CPZ", MODE_ABSOLUTE, cpz_abs, 4, 0, 0, 0, {0xDC}, 1},
	{"CPZ", MODE_ZP, cpz_zp, 3, 0, 0, 0, {0xD4}, 1},
	{"CPX", MODE_IMMEDIATE, cpx_imm, 2, 0, 0, 0, {0xE0}, 1},
	{"CPX", MODE_ZP, cpx_zp, 3, 0, 0, 0, {0xE4}, 1},
	{"CPX", MODE_ABSOLUTE, cpx_abs, 4, 0, 0, 0, {0xEC}, 1},
	{"CPY", MODE_IMMEDIATE, cpy_imm, 2, 0, 0, 0, {0xC0}, 1},
	{"CPY", MODE_ZP, cpy_zp, 3, 0, 0, 0, {0xC4}, 1},
	{"CPY", MODE_ABSOLUTE, cpy_abs, 4, 0, 0, 0, {0xCC}, 1},
	{"INC", MODE_ABSOLUTE, inc_abs, 6, 0, 0, 0, {0xEE}, 1},
	{"INC", MODE_ZP, inc_zp, 5, 0, 0, 0, {0xE6}, 1},
	{"INC", MODE_ZP_X, inc_zp_x, 6, 0, 0, 0, {0xF6}, 1},
	{"INC", MODE_ABSOLUTE_X, inc_abs_x, 7, 0, 0, 0, {0xFE}, 1},
	{"INA", MODE_IMPLIED, ina, 2, 0, 0, 0, {0x1A}, 1},
	{"INX", MODE_IMPLIED, inx, 2, 0, 0, 0, {0xE8}, 1},
	{"INY", MODE_IMPLIED, iny, 2, 0, 0, 0, {0xC8}, 1},
	{"INZ", MODE_IMPLIED, inz, 2, 0, 0, 0, {0x1B}, 1},
	{"DEC", MODE_ABSOLUTE, dec_abs, 6, 0, 0, 0, {0xCE}, 1},
	{"DEC", MODE_ZP, dec_zp, 5, 0, 0, 0, {0xC6}, 1},
	{"DEC", MODE_ZP_X, dec_zp_x, 6, 0, 0, 0, {0xD6}, 1},
	{"DEC", MODE_ABSOLUTE_X, dec_abs_x, 7, 0, 0, 0, {0xDE}, 1},
	{"DEA", MODE_IMPLIED, dea, 2, 0, 0, 0, {0x3A}, 1},
	{"DEX", MODE_IMPLIED, dex, 2, 0, 0, 0, {0xCA}, 1},
	{"DEY", MODE_IMPLIED, dey, 2, 0, 0, 0, {0x88}, 1},
	{"DEZ", MODE_IMPLIED, dez, 2, 0, 0, 0, {0x3B}, 1},
	{"ASL", MODE_ABSOLUTE, asl_abs, 6, 0, 0, 0, {0x0E}, 1},
	{"ASL", MODE_IMPLIED, asla, 2, 0, 0, 0, {0x0A}, 1},
	{"ASL", MODE_ZP, asl_zp, 5, 0, 0, 0, {0x06}, 1},
	{"ASL", MODE_ZP_X, asl_zp_x, 6, 0, 0, 0, {0x16}, 1},
	{"ASL", MODE_ABSOLUTE_X, asl_abs_x, 7, 0, 0, 0, {0x1E}, 1},
	{"LSR", MODE_ABSOLUTE, lsr_abs, 6, 0, 0, 0, {0x4E}, 1},
	{"LSR", MODE_IMPLIED, lsra, 2, 0, 0, 0, {0x4A}, 1},
	{"LSR", MODE_ZP, lsr_zp, 5, 0, 0, 0, {0x46}, 1},
	{"LSR", MODE_ZP_X, lsr_zp_x, 6, 0, 0, 0, {0x56}, 1},
	{"LSR", MODE_ABSOLUTE_X, lsr_abs_x, 7, 0, 0, 0, {0x5E}, 1},
	{"ROL", MODE_ABSOLUTE, rol_abs, 6, 0, 0, 0, {0x2E}, 1},
	{"ROL", MODE_IMPLIED, rola, 2, 0, 0, 0, {0x2A}, 1},
	{"ROL", MODE_ZP, rol_zp, 5, 0, 0, 0, {0x26}, 1},
	{"ROL", MODE_ZP_X, rol_zp_x, 6, 0, 0, 0, {0x36}, 1},
	{"ROL", MODE_ABSOLUTE_X, rol_abs_x, 7, 0, 0, 0, {0x3E}, 1},
	{"ROR", MODE_ABSOLUTE, ror_abs, 6, 0, 0, 0, {0x6E}, 1},
	{"ROR", MODE_IMPLIED, rora, 2, 0, 0, 0, {0x6A}, 1},
	{"ROR", MODE_ZP, ror_zp, 5, 0, 0, 0, {0x66}, 1},
	{"ROR", MODE_ZP_X, ror_zp_x, 6, 0, 0, 0, {0x76}, 1},
	{"ROR", MODE_ABSOLUTE_X, ror_abs_x, 7, 0, 0, 0, {0x7E}, 1},
	{"AND", MODE_IMMEDIATE, and_imm, 2, 0, 0, 0, {0x29}, 1},
	{"AND", MODE_ABSOLUTE, and_abs, 4, 0, 0, 0, {0x2D}, 1},
	{"AND", MODE_ZP, and_zp, 3, 0, 0, 0, {0x25}, 1},
	{"AND", MODE_ZP_X, and_zp_x, 4, 0, 0, 0, {0x35}, 1},
	{"AND", MODE_INDIRECT_X, and_ind_x, 6, 0, 0, 0, {0x21}, 1},
	{"AND", MODE_INDIRECT_Y, and_ind_y, 5, 0, 0, 0, {0x31}, 1},
	{"AND", MODE_ABSOLUTE_Y, and_abs_y, 4, 0, 0, 0, {0x39}, 1},
	{"AND", MODE_ABSOLUTE_X, and_abs_x, 4, 0, 0, 0, {0x3D}, 1},
	{"EOR", MODE_IMMEDIATE, eor_imm, 2, 0, 0, 0, {0x49}, 1},
	{"EOR", MODE_ABSOLUTE, eor_abs, 4, 0, 0, 0, {0x4D}, 1},
	{"EOR", MODE_ZP, eor_zp, 3, 0, 0, 0, {0x45}, 1},
	{"EOR", MODE_ZP_X, eor_zp_x, 4, 0, 0, 0, {0x55}, 1},
	{"EOR", MODE_INDIRECT_X, eor_ind_x, 6, 0, 0, 0, {0x41}, 1},
	{"EOR", MODE_INDIRECT_Y, eor_ind_y, 5, 0, 0, 0, {0x51}, 1},
	{"EOR", MODE_ABSOLUTE_Y, eor_abs_y, 4, 0, 0, 0, {0x59}, 1},
	{"EOR", MODE_ABSOLUTE_X, eor_abs_x, 4, 0, 0, 0, {0x5D}, 1},
	{"ORA", MODE_IMMEDIATE, ora_imm, 2, 0, 0, 0, {0x09}, 1},
	{"ORA", MODE_ABSOLUTE, ora_abs, 4, 0, 0, 0, {0x0D}, 1},
	{"ORA", MODE_ZP, ora_zp, 3, 0, 0, 0, {0x05}, 1},
	{"ORA", MODE_ZP_X, ora_zp_x, 4, 0, 0, 0, {0x15}, 1},
	{"ORA", MODE_INDIRECT_X, ora_ind_x, 6, 0, 0, 0, {0x01}, 1},
	{"ORA", MODE_INDIRECT_Y, ora_ind_y, 5, 0, 0, 0, {0x11}, 1},
	{"ORA", MODE_ABSOLUTE_Y, ora_abs_y, 4, 0, 0, 0, {0x19}, 1},
	{"ORA", MODE_ABSOLUTE_X, ora_abs_x, 4, 0, 0, 0, {0x1D}, 1},
	{"BIT", MODE_ZP, bit_zp, 3, 0, 0, 0, {0x24}, 1},
	{"BIT", MODE_ABSOLUTE, bit_abs, 4, 0, 0, 0, {0x2C}, 1},
	{"BIT", MODE_IMMEDIATE, bit_imm, 2, 0, 0, 0, {0x89}, 1},
	{"BIT", MODE_ZP_X, bit_zp_x, 4, 0, 0, 0, {0x34}, 1},
	{"BIT", MODE_ABSOLUTE_X, bit_abs_x, 4, 0, 0, 0, {0x3C}, 1},
	{"TRB", MODE_ZP, trb_zp, 5, 0, 0, 0, {0x14}, 1},
	{"TRB", MODE_ABSOLUTE, trb_abs, 6, 0, 0, 0, {0x1C}, 1},
	{"TSB", MODE_ZP, tsb_zp, 5, 0, 0, 0, {0x04}, 1},
	{"TSB", MODE_ABSOLUTE, tsb_abs, 6, 0, 0, 0, {0x0C}, 1},
	{"RMB0", MODE_ZP, rmb0, 5, 0, 0, 0, {0x07}, 1},
	{"RMB1", MODE_ZP, rmb1, 5, 0, 0, 0, {0x17}, 1},
	{"RMB2", MODE_ZP, rmb2, 5, 0, 0, 0, {0x27}, 1},
	{"RMB3", MODE_ZP, rmb3, 5, 0, 0, 0, {0x37}, 1},
	{"RMB4", MODE_ZP, rmb4, 5, 0, 0, 0, {0x47}, 1},
	{"RMB5", MODE_ZP, rmb5, 5, 0, 0, 0, {0x57}, 1},
	{"RMB6", MODE_ZP, rmb6, 5, 0, 0, 0, {0x67}, 1},
	{"RMB7", MODE_ZP, rmb7, 5, 0, 0, 0, {0x77}, 1},
	{"SMB0", MODE_ZP, smb0, 5, 0, 0, 0, {0x87}, 1},
	{"SMB1", MODE_ZP, smb1, 5, 0, 0, 0, {0x97}, 1},
	{"SMB2", MODE_ZP, smb2, 5, 0, 0, 0, {0xA7}, 1},
	{"SMB3", MODE_ZP, smb3, 5, 0, 0, 0, {0xB7}, 1},
	{"SMB4", MODE_ZP, smb4, 5, 0, 0, 0, {0xC7}, 1},
	{"SMB5", MODE_ZP, smb5, 5, 0, 0, 0, {0xD7}, 1},
	{"SMB6", MODE_ZP, smb6, 5, 0, 0, 0, {0xE7}, 1},
	{"SMB7", MODE_ZP, smb7, 5, 0, 0, 0, {0xF7}, 1},
	{"BBR0", MODE_ZP, bbr0, 5, 0, 0, 0, {0x0F}, 1},
	{"BBR1", MODE_ZP, bbr1, 5, 0, 0, 0, {0x1F}, 1},
	{"BBR2", MODE_ZP, bbr2, 5, 0, 0, 0, {0x2F}, 1},
	{"BBR3", MODE_ZP, bbr3, 5, 0, 0, 0, {0x3F}, 1},
	{"BBR4", MODE_ZP, bbr4, 5, 0, 0, 0, {0x4F}, 1},
	{"BBR5", MODE_ZP, bbr5, 5, 0, 0, 0, {0x5F}, 1},
	{"BBR6", MODE_ZP, bbr6, 5, 0, 0, 0, {0x6F}, 1},
	{"BBR7", MODE_ZP, bbr7, 5, 0, 0, 0, {0x7F}, 1},
	{"BBS0", MODE_ZP, bbs0, 5, 0, 0, 0, {0x8F}, 1},
	{"BBS1", MODE_ZP, bbs1, 5, 0, 0, 0, {0x9F}, 1},
	{"BBS2", MODE_ZP, bbs2, 5, 0, 0, 0, {0xAF}, 1},
	{"BBS3", MODE_ZP, bbs3, 5, 0, 0, 0, {0xBF}, 1},
	{"BBS4", MODE_ZP, bbs4, 5, 0, 0, 0, {0xCF}, 1},
	{"BBS5", MODE_ZP, bbs5, 5, 0, 0, 0, {0xDF}, 1},
	{"BBS6", MODE_ZP, bbs6, 5, 0, 0, 0, {0xEF}, 1},
	{"BBS7", MODE_ZP, bbs7, 5, 0, 0, 0, {0xFF}, 1},
	{"STZ", MODE_ABSOLUTE, stz_abs, 4, 0, 0, 0, {0x9C}, 1},
	{"STZ", MODE_ABSOLUTE_X, stz_abs_x, 5, 0, 0, 0, {0x9E}, 1},
	{"STZ", MODE_ZP, stz_zp, 3, 0, 0, 0, {0x64}, 1},
	{"STZ", MODE_ZP_X, stz_zp_x, 4, 0, 0, 0, {0x74}, 1},
	{"BRA", MODE_RELATIVE, bra_rel, 2, 0, 0, 0, {0x80}, 1},
	{"BRA", MODE_RELATIVE_LONG, bra_relfar, 3, 0, 0, 0, {0x83}, 1},
	{"BRL", MODE_RELATIVE_LONG, bra_relfar, 3, 0, 0, 0, {0x83}, 1},
	{"LBRA", MODE_RELATIVE_LONG, bra_relfar, 3, 0, 0, 0, {0x83}, 1},
	{"JMP", MODE_ABSOLUTE, jmp_abs, 3, 0, 0, 0, {0x4C}, 1},
	{"JMP", MODE_INDIRECT, jmp_abs_indirect, 5, 0, 0, 0, {0x6C}, 1},
	{"JMP", MODE_INDIRECT_X, jmp_ind_x, 6, 0, 0, 0, {0x7C}, 1},
	{"JSR", MODE_ABSOLUTE, jsr_abs, 6, 0, 0, 0, {0x20}, 1},
	{"JSR", MODE_INDIRECT, jsr_ind, 5, 0, 0, 0, {0x22}, 1},
	{"JSR", MODE_ABS_INDIRECT_X, jsr_ind_x, 5, 0, 0, 0, {0x23}, 1},
	{"RTS", MODE_IMPLIED, rts, 6, 0, 0, 0, {0x60}, 1},
	{"BCC", MODE_RELATIVE, bcc, 2, 0, 0, 0, {0x90}, 1},
	{"BCC", MODE_RELATIVE_LONG, bcc_relfar, 3, 0, 0, 0, {0x93}, 1},
	{"LBCC", MODE_RELATIVE_LONG, bcc_relfar, 3, 0, 0, 0, {0x93}, 1},
	{"BCS", MODE_RELATIVE, bcs, 2, 0, 0, 0, {0xB0}, 1},
	{"BCS", MODE_RELATIVE_LONG, bcs_relfar, 3, 0, 0, 0, {0xB3}, 1},
	{"LBCS", MODE_RELATIVE_LONG, bcs_relfar, 3, 0, 0, 0, {0xB3}, 1},
	{"BEQ", MODE_RELATIVE, beq, 2, 0, 0, 0, {0xF0}, 1},
	{"BEQ", MODE_RELATIVE_LONG, beq_relfar, 3, 0, 0, 0, {0xF3}, 1},
	{"LBEQ", MODE_RELATIVE_LONG, beq_relfar, 3, 0, 0, 0, {0xF3}, 1},
	{"BNE", MODE_RELATIVE, bne, 2, 0, 0, 0, {0xD0}, 1},
	{"BNE", MODE_RELATIVE_LONG, bne_relfar, 3, 0, 0, 0, {0xD3}, 1},
	{"LBNE", MODE_RELATIVE_LONG, bne_relfar, 3, 0, 0, 0, {0xD3}, 1},
	{"BMI", MODE_RELATIVE, bmi, 2, 0, 0, 0, {0x30}, 1},
	{"BMI", MODE_RELATIVE_LONG, bmi_relfar, 3, 0, 0, 0, {0x33}, 1},
	{"LBMI", MODE_RELATIVE_LONG, bmi_relfar, 3, 0, 0, 0, {0x33}, 1},
	{"BPL", MODE_RELATIVE, bpl, 2, 0, 0, 0, {0x10}, 1},
	{"BPL", MODE_RELATIVE_LONG, bpl_relfar, 3, 0, 0, 0, {0x13}, 1},
	{"LBPL", MODE_RELATIVE_LONG, bpl_relfar, 3, 0, 0, 0, {0x13}, 1},
	{"BVS", MODE_RELATIVE, bvs, 2, 0, 0, 0, {0x70}, 1},
	{"BVS", MODE_RELATIVE_LONG, bvs_relfar, 3, 0, 0, 0, {0x73}, 1},
	{"LBVS", MODE_RELATIVE_LONG, bvs_relfar, 3, 0, 0, 0, {0x73}, 1},
	{"BVC", MODE_RELATIVE, bvc, 2, 0, 0, 0, {0x50}, 1},
	{"BVC", MODE_RELATIVE_LONG, bvc_relfar, 3, 0, 0, 0, {0x53}, 1},
	{"LBVC", MODE_RELATIVE_LONG, bvc_relfar, 3, 0, 0, 0, {0x53}, 1},
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
	{"PHA", MODE_IMPLIED, pha_45gs02, 3, 0, 0, 0, {0x48}, 1},
	{"PLA", MODE_IMPLIED, pla_45gs02, 4, 0, 0, 0, {0x68}, 1},
	{"PHX", MODE_IMPLIED, phx_45gs02, 3, 0, 0, 0, {0xDA}, 1},
	{"PLX", MODE_IMPLIED, plx_45gs02, 4, 0, 0, 0, {0xFA}, 1},
	{"PHY", MODE_IMPLIED, phy_45gs02, 3, 0, 0, 0, {0x5A}, 1},
	{"PLY", MODE_IMPLIED, ply_45gs02, 4, 0, 0, 0, {0x7A}, 1},
	{"PHP", MODE_IMPLIED, php_45gs02, 3, 0, 0, 0, {0x08}, 1},
	{"PLP", MODE_IMPLIED, plp_45gs02, 4, 0, 0, 0, {0x28}, 1},
	{"BRK", MODE_IMPLIED, brk_45gs02, 7, 0, 0, 0, {0x00}, 1},
	{"RTI", MODE_IMPLIED, rti_45gs02, 6, 0, 0, 0, {0x40}, 1},
	{"EOM", MODE_IMPLIED, eom, 2, 0, 0, 0, {0xEA}, 1},
	{"NOP", MODE_IMPLIED, op_nop, 2, 0, 0, 0, {0xEA}, 0},
	/* Quad (32-bit) instructions - NEG NEG ($42 $42) prefix */
	{"LDQ", MODE_ZP, ldq_zp, 5, 0, 0, 0, {0x42, 0x42, 0xA5}, 3},
	{"LDQ", MODE_ABSOLUTE, ldq_abs, 5, 0, 0, 0, {0x42, 0x42, 0xAD}, 3},
	{"LDQ", MODE_ZP_INDIRECT_Z, ldq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0xB2}, 3},
	{"STQ", MODE_ZP, stq_zp, 5, 0, 0, 0, {0x42, 0x42, 0x85}, 3},
	{"STQ", MODE_ABSOLUTE, stq_abs, 5, 0, 0, 0, {0x42, 0x42, 0x8D}, 3},
	{"STQ", MODE_ZP_INDIRECT_Z, stq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0x92}, 3},
	/* Quad flat [zp],Z — $42 $42 $EA + base opcode; eom_prefix set to 2 by dispatcher */
	{"LDQ", MODE_ZP_INDIRECT_Z_FLAT, ldq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0xEA, 0xB2}, 4},
	{"STQ", MODE_ZP_INDIRECT_Z_FLAT, stq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0xEA, 0x92}, 4},
	{"ADCQ", MODE_ZP, adcq_zp, 5, 0, 0, 0, {0x42, 0x42, 0x65}, 3},
	{"ADCQ", MODE_ABSOLUTE, adcq_abs, 5, 0, 0, 0, {0x42, 0x42, 0x6D}, 3},
	{"SBCQ", MODE_ZP, sbcq_zp, 5, 0, 0, 0, {0x42, 0x42, 0xE5}, 3},
	{"SBCQ", MODE_ABSOLUTE, sbcq_abs, 5, 0, 0, 0, {0x42, 0x42, 0xED}, 3},
	{"CPQ", MODE_ZP, cpq_zp, 5, 0, 0, 0, {0x42, 0x42, 0xC5}, 3},
	{"CPQ", MODE_ABSOLUTE, cpq_abs, 5, 0, 0, 0, {0x42, 0x42, 0xCD}, 3},
	{"EORQ", MODE_ZP, eorq_zp, 5, 0, 0, 0, {0x42, 0x42, 0x45}, 3},
	{"EORQ", MODE_ABSOLUTE, eorq_abs, 5, 0, 0, 0, {0x42, 0x42, 0x4D}, 3},
	{"ANDQ", MODE_ZP, andq_zp, 5, 0, 0, 0, {0x42, 0x42, 0x25}, 3},
	{"ANDQ", MODE_ABSOLUTE, andq_abs, 5, 0, 0, 0, {0x42, 0x42, 0x2D}, 3},
	{"BITQ", MODE_ZP, bitq_zp, 5, 0, 0, 0, {0x42, 0x42, 0x24}, 3},
	{"BITQ", MODE_ABSOLUTE, bitq_abs, 5, 0, 0, 0, {0x42, 0x42, 0x2C}, 3},
	{"ORQ", MODE_ZP, orq_zp, 5, 0, 0, 0, {0x42, 0x42, 0x05}, 3},
	{"ORQ", MODE_ABSOLUTE, orq_abs, 5, 0, 0, 0, {0x42, 0x42, 0x0D}, 3},
	{"ASLQ", MODE_IMPLIED, aslq, 2, 0, 0, 0, {0x42, 0x42, 0x0A}, 3},
	{"ASLQ", MODE_ZP, aslq_zp, 7, 0, 0, 0, {0x42, 0x42, 0x06}, 3},
	{"ASLQ", MODE_ZP_X, aslq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0x16}, 3},
	{"ASLQ", MODE_ABSOLUTE, aslq_abs, 8, 0, 0, 0, {0x42, 0x42, 0x0E}, 3},
	{"ASLQ", MODE_ABSOLUTE_X, aslq_abs_x, 9, 0, 0, 0, {0x42, 0x42, 0x1E}, 3},
	{"ASRQ", MODE_IMPLIED, asrq, 2, 0, 0, 0, {0x42, 0x42, 0x43}, 3},
	{"ASRQ", MODE_ZP, asrq_zp, 7, 0, 0, 0, {0x42, 0x42, 0x44}, 3},
	{"ASRQ", MODE_ZP_X, asrq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0x54}, 3},
	{"LSRQ", MODE_IMPLIED, lsrq, 2, 0, 0, 0, {0x42, 0x42, 0x4A}, 3},
	{"LSRQ", MODE_ZP, lsrq_zp, 7, 0, 0, 0, {0x42, 0x42, 0x46}, 3},
	{"LSRQ", MODE_ZP_X, lsrq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0x56}, 3},
	{"LSRQ", MODE_ABSOLUTE, lsrq_abs, 8, 0, 0, 0, {0x42, 0x42, 0x4E}, 3},
	{"LSRQ", MODE_ABSOLUTE_X, lsrq_abs_x, 9, 0, 0, 0, {0x42, 0x42, 0x5E}, 3},
	{"ROLQ", MODE_IMPLIED, rolq, 2, 0, 0, 0, {0x42, 0x42, 0x2A}, 3},
	{"ROLQ", MODE_ZP, rolq_zp, 7, 0, 0, 0, {0x42, 0x42, 0x26}, 3},
	{"ROLQ", MODE_ZP_X, rolq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0x36}, 3},
	{"ROLQ", MODE_ABSOLUTE, rolq_abs, 8, 0, 0, 0, {0x42, 0x42, 0x2E}, 3},
	{"ROLQ", MODE_ABSOLUTE_X, rolq_abs_x, 9, 0, 0, 0, {0x42, 0x42, 0x3E}, 3},
	{"RORQ", MODE_IMPLIED, rorq, 2, 0, 0, 0, {0x42, 0x42, 0x6A}, 3},
	{"RORQ", MODE_ZP, rorq_zp, 7, 0, 0, 0, {0x42, 0x42, 0x66}, 3},
	{"RORQ", MODE_ZP_X, rorq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0x76}, 3},
	{"RORQ", MODE_ABSOLUTE, rorq_abs, 8, 0, 0, 0, {0x42, 0x42, 0x6E}, 3},
	{"RORQ", MODE_ABSOLUTE_X, rorq_abs_x, 9, 0, 0, 0, {0x42, 0x42, 0x7E}, 3},
	{"INQ", MODE_IMPLIED, inq, 2, 0, 0, 0, {0x42, 0x42, 0x1A}, 3},
	{"INQ", MODE_ZP, inq_zp, 7, 0, 0, 0, {0x42, 0x42, 0xE6}, 3},
	{"INQ", MODE_ZP_X, inq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0xF6}, 3},
	{"INQ", MODE_ABSOLUTE, inq_abs, 8, 0, 0, 0, {0x42, 0x42, 0xEE}, 3},
	{"INQ", MODE_ABSOLUTE_X, inq_abs_x, 9, 0, 0, 0, {0x42, 0x42, 0xFE}, 3},
	{"DEQ", MODE_IMPLIED, deq, 2, 0, 0, 0, {0x42, 0x42, 0x3A}, 3},
	{"DEQ", MODE_ZP, deq_zp, 7, 0, 0, 0, {0x42, 0x42, 0xC6}, 3},
	{"DEQ", MODE_ZP_X, deq_zp_x, 8, 0, 0, 0, {0x42, 0x42, 0xD6}, 3},
	{"DEQ", MODE_ABSOLUTE, deq_abs, 8, 0, 0, 0, {0x42, 0x42, 0xCE}, 3},
	{"DEQ", MODE_ABSOLUTE_X, deq_abs_x, 9, 0, 0, 0, {0x42, 0x42, 0xDE}, 3},
	/* Quad indirect-Z ALU variants */
	{"ADCQ", MODE_ZP_INDIRECT_Z, adcq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0x72}, 3},
	{"ADCQ", MODE_ZP_INDIRECT_Z_FLAT, adcq_zp_ind_z, 11, 0, 0, 0, {0x42, 0x42, 0xEA, 0x72}, 4},
	{"SBCQ", MODE_ZP_INDIRECT_Z, sbcq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0xF2}, 3},
	{"SBCQ", MODE_ZP_INDIRECT_Z_FLAT, sbcq_zp_ind_z, 11, 0, 0, 0, {0x42, 0x42, 0xEA, 0xF2}, 4},
	{"CPQ", MODE_ZP_INDIRECT_Z, cmpq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0xD2}, 3},
	{"CPQ", MODE_ZP_INDIRECT_Z_FLAT, cmpq_zp_ind_z, 11, 0, 0, 0, {0x42, 0x42, 0xEA, 0xD2}, 4},
	{"ANDQ", MODE_ZP_INDIRECT_Z, andq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0x32}, 3},
	{"ANDQ", MODE_ZP_INDIRECT_Z_FLAT, andq_zp_ind_z, 11, 0, 0, 0, {0x42, 0x42, 0xEA, 0x32}, 4},
	{"EORQ", MODE_ZP_INDIRECT_Z, eorq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0x52}, 3},
	{"EORQ", MODE_ZP_INDIRECT_Z_FLAT, eorq_zp_ind_z, 11, 0, 0, 0, {0x42, 0x42, 0xEA, 0x52}, 4},
	{"ORQ", MODE_ZP_INDIRECT_Z, orq_zp_ind_z, 9, 0, 0, 0, {0x42, 0x42, 0x12}, 3},
	{"ORQ", MODE_ZP_INDIRECT_Z_FLAT, orq_zp_ind_z, 11, 0, 0, 0, {0x42, 0x42, 0xEA, 0x12}, 4},
};

int OPCODES_45GS02_COUNT = sizeof(opcodes_45gs02) / sizeof(opcodes_45gs02[0]);
