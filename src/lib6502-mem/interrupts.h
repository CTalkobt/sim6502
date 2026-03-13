#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "cpu.h"
#include <stdint.h>

/* Interrupt types */
typedef enum {
	INT_NONE = 0,
	INT_IRQ = 1,    /* Maskable interrupt */
	INT_NMI = 2,    /* Non-maskable interrupt */
	INT_BRK = 3,    /* Software break */
	INT_RESET = 4   /* System reset */
} interrupt_type_t;

/* Interrupt vector addresses */
#define VECTOR_NMI      0xFFFA   /* Non-maskable interrupt */
#define VECTOR_RESET    0xFFFC   /* Reset */
#define VECTOR_IRQ      0xFFFE   /* IRQ/BRK */

typedef struct {
	interrupt_type_t type;
	uint16_t vector;
	int pending;
	unsigned long cycle_triggered;
} interrupt_state_t;

typedef struct {
	interrupt_state_t current;
	int irq_line;          /* 1 = IRQ pin asserted (active low in hardware, here 1 = trigger) */
	int nmi_pending;       /* 1 = NMI edge detected */
	int in_handler;        /* 1 = currently handling interrupt */
	unsigned int handled_count;
} interrupt_controller_t;

/* Initialize interrupt controller */
void interrupt_init(interrupt_controller_t *handler);

/* Request IRQ (maskable) */
void interrupt_request_irq(interrupt_controller_t *handler);

/* Release IRQ line */
void interrupt_release_irq(interrupt_controller_t *handler);

/* Request NMI (non-maskable) - triggers on falling edge */
void interrupt_request_nmi(interrupt_controller_t *handler);

/* Request Reset */
void interrupt_request_reset(interrupt_controller_t *handler);

/* Check if interrupt should be taken */
int interrupt_check(interrupt_controller_t *handler, cpu_t *cpu);

/* Execute interrupt handler */
void interrupt_handle(interrupt_controller_t *handler, cpu_t *cpu, memory_t *mem);

/* Return from interrupt (RTI) */
void interrupt_return(interrupt_controller_t *handler, cpu_t *cpu, memory_t *mem);

/* Helper functions */
const char *interrupt_name(interrupt_type_t type);
void interrupt_display_status(const interrupt_controller_t *handler);

#endif
