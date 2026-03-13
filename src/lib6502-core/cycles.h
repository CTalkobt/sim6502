#ifndef CYCLES_H
#define CYCLES_H

#include "cpu.h"

/* Cycle count matrices for each processor variant */
/* Based on official 6502/65C02/65CE02/45GS02 datasheets */

/* 6502 Cycle counts */
#define C6502_LDA_IMM 2
#define C6502_LDA_ABS 4
#define C6502_LDA_ABS_X 4
#define C6502_LDA_ABS_Y 4
#define C6502_LDA_ZP 3
#define C6502_LDA_ZP_X 4
#define C6502_LDA_IND_X 6
#define C6502_LDA_IND_Y 5
#define C6502_STA_ABS 4
#define C6502_STA_ABS_X 5
#define C6502_STA_ABS_Y 5
#define C6502_STA_ZP 3
#define C6502_STA_ZP_X 4
#define C6502_STA_IND_X 6
#define C6502_STA_IND_Y 6
#define C6502_ADC_IMM 2
#define C6502_ADC_ABS 4
#define C6502_ADC_ABS_X 4
#define C6502_ADC_ABS_Y 4
#define C6502_ADC_ZP 3
#define C6502_ADC_ZP_X 4
#define C6502_ADC_IND_X 6
#define C6502_ADC_IND_Y 5
#define C6502_INC_ABS 6
#define C6502_INC_ZP 5
#define C6502_DEC_ABS 6
#define C6502_DEC_ZP 5
#define C6502_ASL_ABS 6
#define C6502_ASL_A 2
#define C6502_LSR_ABS 6
#define C6502_LSR_A 2
#define C6502_ROL_ABS 6
#define C6502_ROL_A 2
#define C6502_ROR_ABS 6
#define C6502_ROR_A 2
#define C6502_CMP_IMM 2
#define C6502_CMP_ABS 4
#define C6502_CMP_ZP 3
#define C6502_CPX_IMM 2
#define C6502_CPX_ABS 4
#define C6502_CPX_ZP 3
#define C6502_CPY_IMM 2
#define C6502_CPY_ABS 4
#define C6502_CPY_ZP 3
#define C6502_BIT_ABS 4
#define C6502_BIT_ZP 3
#define C6502_AND_IMM 2
#define C6502_AND_ABS 4
#define C6502_AND_ZP 3
#define C6502_EOR_IMM 2
#define C6502_EOR_ABS 4
#define C6502_EOR_ZP 3
#define C6502_ORA_IMM 2
#define C6502_ORA_ABS 4
#define C6502_ORA_ZP 3
#define C6502_BCC 2
#define C6502_BCS 2
#define C6502_BEQ 2
#define C6502_BNE 2
#define C6502_BMI 2
#define C6502_BPL 2
#define C6502_BVS 2
#define C6502_BVC 2
#define C6502_JMP_ABS 3
#define C6502_JMP_IND 5
#define C6502_JSR_ABS 6
#define C6502_RTS 6
#define C6502_RTI 6
#define C6502_BRK 7
#define C6502_NOP 2
#define C6502_TAX 2
#define C6502_TXA 2
#define C6502_TAY 2
#define C6502_TYA 2
#define C6502_TSX 2
#define C6502_TXS 2
#define C6502_PHA 3
#define C6502_PLA 4
#define C6502_PHP 3
#define C6502_PLP 4
#define C6502_CLC 2
#define C6502_SEC 2
#define C6502_CLD 2
#define C6502_SED 2
#define C6502_CLI 2
#define C6502_SEI 2
#define C6502_CLV 2
#define C6502_INX 2
#define C6502_INY 2
#define C6502_DEX 2
#define C6502_DEY 2
#define C6502_INA 2
#define C6502_DEA 2
#define C6502_LDX_IMM 2
#define C6502_LDX_ABS 4
#define C6502_LDX_ABS_Y 4
#define C6502_LDX_ZP 3
#define C6502_LDX_ZP_Y 4
#define C6502_LDY_IMM 2
#define C6502_LDY_ABS 4
#define C6502_LDY_ABS_X 4
#define C6502_LDY_ZP 3
#define C6502_LDY_ZP_X 4
#define C6502_STX_ABS 4
#define C6502_STX_ZP 3
#define C6502_STX_ZP_Y 4
#define C6502_STY_ABS 4
#define C6502_STY_ZP 3
#define C6502_STY_ZP_X 4
#define C6502_SBC_IMM 2
#define C6502_SBC_ABS 4
#define C6502_SBC_ZP 3

/* 65C02 - Some improvements in cycle counts */
#define C65C02_BIT_IMM 2      /* New on 65C02 */
#define C65C02_TSB_ZP 5       /* New on 65C02 */
#define C65C02_TSB_ABS 6      /* New on 65C02 */
#define C65C02_TRB_ZP 5       /* New on 65C02 */
#define C65C02_TRB_ABS 6      /* New on 65C02 */
#define C65C02_STZ_ABS 4      /* New on 65C02 */
#define C65C02_STZ_ABS_X 5    /* New on 65C02 */
#define C65C02_STZ_ZP 3       /* New on 65C02 */
#define C65C02_STZ_ZP_X 4     /* New on 65C02 */
#define C65C02_BRA 3          /* New on 65C02 */
#define C65C02_BRL 4          /* New on 65C02 (16-bit branch) */
#define C65C02_SMB0 5         /* New on 65C02 */
#define C65C02_RMB0 5         /* New on 65C02 */
#define C65C02_BBR0 5         /* New on 65C02 */
#define C65C02_BBS0 5         /* New on 65C02 */

/* 65CE02 additions */
#define C65CE02_PHX 3
#define C65CE02_PLX 4
#define C65CE02_PHY 3
#define C65CE02_PLY 4
#define C65CE02_JMP_ABS_IND 6      /* Absolute Indirect */
#define C65CE02_JMP_ABS_IND_X 6    /* Absolute Indirect,X */
#define C65CE02_JSR_ABS_IND_X 8    /* JSR with indirect,X */

/* 45GS02 additions */
#define C45GS02_LDZ_IMM 2
#define C45GS02_LDZ_ABS 4
#define C45GS02_LDZ_ZP 3
#define C45GS02_STZ_Z_ABS 4
#define C45GS02_STZ_Z_ZP 3
#define C45GS02_INZ 2
#define C45GS02_DEZ 2
#define C45GS02_CPZ_IMM 2
#define C45GS02_CPZ_ABS 4
#define C45GS02_CPZ_ZP 3
#define C45GS02_TZX 2
#define C45GS02_TXZ 2
#define C45GS02_TZY 2
#define C45GS02_TYZ 2

/* Check for page boundary crossing */
static inline int page_crossed(unsigned short addr1, unsigned short addr2) {
	return (addr1 & 0xFF00) != (addr2 & 0xFF00);
}

/* Get cycle count with page crossing penalty */
static inline unsigned char get_cycles_with_penalty(unsigned char base_cycles, 
	unsigned short addr, unsigned char index, int penalty_enabled) {
	if (!penalty_enabled) return base_cycles;
	
	/* Check if indexing crosses page boundary */
	if (page_crossed(addr, addr + index)) {
		return base_cycles + 1;
	}
	return base_cycles;
}

#endif
