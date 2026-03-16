#include "sim_api.h"
#include "interrupts.h"
#include "memory.h"
#include <stdio.h>
#include <stdint.h>

void interrupt_init(interrupt_controller_t *handler) {
	handler->current.type = INT_NONE;
	handler->current.vector = 0;
	handler->current.pending = 0;
	handler->current.cycle_triggered = 0;
	handler->irq_line = 0;
	handler->nmi_pending = 0;
	handler->in_handler = 0;
	handler->handled_count = 0;
}

void interrupt_request_irq(interrupt_controller_t *handler) {
	handler->irq_line = 1;
}

void interrupt_release_irq(interrupt_controller_t *handler) {
	handler->irq_line = 0;
}

void interrupt_request_nmi(interrupt_controller_t *handler) {
	handler->nmi_pending = 1;
}

void interrupt_request_reset(interrupt_controller_t *handler) {
	handler->current.type = INT_RESET;
	handler->current.vector = VECTOR_RESET;
	handler->current.pending = 1;
}

int interrupt_check(interrupt_controller_t *handler, cpu_t *cpu) {
	/* RESET has priority */
	if (handler->current.type == INT_RESET && handler->current.pending) {
		return 1;
	}

	/* NMI has priority over IRQ */
	if (handler->nmi_pending) {
		handler->current.type = INT_NMI;
		handler->current.vector = VECTOR_NMI;
		handler->current.pending = 1;
		handler->nmi_pending = 0;
		return 1;
	}
	
	/* IRQ if line asserted and I flag clear */
	if (handler->irq_line && !(cpu->p & 0x04)) {  /* I flag = bit 2 */
		handler->current.type = INT_IRQ;
		handler->current.vector = VECTOR_IRQ;
		handler->current.pending = 1;
		return 1;
	}
	
	return 0;
}

void interrupt_handle(interrupt_controller_t *handler, cpu_t *cpu, memory_t *mem) {
	if (!handler->current.pending) return;

	if (handler->current.type == INT_RESET) {
		uint8_t lo = mem_read(mem, VECTOR_RESET);
		uint8_t hi = mem_read(mem, VECTOR_RESET + 1);
		cpu->pc = lo | (hi << 8);
		handler->current.pending = 0;
		cpu->cycles += 7;
		return;
	}
	
	/* Push return address (PC) to stack */
	mem_write(mem, 0x0100 + cpu->s, (cpu->pc >> 8) & 0xFF);
	cpu->s--;
	
	mem_write(mem, 0x0100 + cpu->s, cpu->pc & 0xFF);
	cpu->s--;
	
	/* Push processor status — B flag only appears in the pushed value, never in cpu->p */
	unsigned char status = cpu->p;
	status |= 0x20;   /* bit 5 always 1 on 6502/65c02 */
	if (handler->current.type == INT_BRK) {
		status |= 0x10;  /* B=1 for BRK/PHP */
	} else {
		status &= ~0x10; /* B=0 for IRQ/NMI */
	}

	mem_write(mem, 0x0100 + cpu->s, status);
	cpu->s--;
	
	/* Set I flag (disable further IRQs) */
	cpu->p |= 0x04;
	
	/* Clear D flag (decimal mode) if not 6502 (or as standard for 65c02+) */
	cpu->p &= ~0x08;
	
	/* Read interrupt vector and jump */
	unsigned char vec_lo = mem_read(mem, handler->current.vector);
	unsigned char vec_hi = mem_read(mem, handler->current.vector + 1);
	cpu->pc = vec_lo | (vec_hi << 8);
	
	/* Add cycles for interrupt handling */
	cpu->cycles += 7;
	
	handler->in_handler = 1;
	handler->handled_count++;
	handler->current.pending = 0;
}

void interrupt_return(interrupt_controller_t *handler, cpu_t *cpu, memory_t *mem) {
	/* Pop processor status */
	cpu->s++;
	cpu->p = mem_read(mem, 0x0100 + cpu->s);
	
	/* Pop return address */
	cpu->s++;
	unsigned char pc_low = mem_read(mem, 0x0100 + cpu->s);
	
	cpu->s++;
	unsigned char pc_high = mem_read(mem, 0x0100 + cpu->s);
	
	cpu->pc = pc_low | (pc_high << 8);
	
	/* Add cycles for RTI */
	cpu->cycles += 6;
	
	handler->in_handler = 0;
}

const char *interrupt_name(interrupt_type_t type) {
	switch (type) {
	case INT_NONE: return "None";
	case INT_IRQ: return "IRQ (Maskable)";
	case INT_NMI: return "NMI (Non-Maskable)";
	case INT_BRK: return "BRK (Software)";
	case INT_RESET: return "RESET";
	default: return "Unknown";
	}
}

void interrupt_display_status(const interrupt_controller_t *handler) {
	cli_printf("\n=== Interrupt Status ===\n");
	cli_printf("Current Type: %s\n", interrupt_name(handler->current.type));
	if (handler->current.type != INT_NONE) {
		cli_printf("Vector: 0x%04X\n", handler->current.vector);
		cli_printf("Pending: %s\n", handler->current.pending ? "Yes" : "No");
	}
	cli_printf("IRQ Line: %s\n", handler->irq_line ? "Asserted" : "Released");
	cli_printf("NMI Pending: %s\n", handler->nmi_pending ? "Yes" : "No");
	cli_printf("Handled: %u\n", handler->handled_count);
	cli_printf("=======================\n");
}
