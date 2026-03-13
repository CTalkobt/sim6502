#ifndef CPU_OBSERVER_H
#define CPU_OBSERVER_H

#include <stdint.h>
#include "cpu_state.h"
#include "memory_types.h"

/*
 * ExecutionObserver — pure abstract hook for CPU execution events.
 * Attach an instance to CPU::observer to receive notifications
 * after each instruction executes.
 */
class ExecutionObserver {
public:
    virtual ~ExecutionObserver() {}

    /* Called after each instruction completes.
     *   pre_pc       : program counter before the instruction
     *   pre_state    : full CPU state before the instruction
     *   post_state   : full CPU state after the instruction
     *   cycles_delta : cycles consumed by this instruction
     *   mem          : memory bus (mem_writes / mem_addr[] etc. are populated) */
    virtual void on_after_execute(uint16_t pre_pc,
                                   const CPUState &pre_state,
                                   const CPUState &post_state,
                                   uint64_t cycles_delta,
                                   memory_t *mem) = 0;
};

#endif
