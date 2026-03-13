#include "cpu_6502.h"
#include <stdlib.h>
#include "cpu_engine.h"
#include "opcodes/opcodes.h"
#include "interrupts.h"
#include <string.h>

CPU6502::CPU6502() {
    memset(&dt, 0, sizeof(dt));
    dispatch_build(&dt, opcodes_6502, OPCODES_6502_COUNT, CPU_6502);
    interrupt_controller_t* ic_ptr = (interrupt_controller_t*)malloc(sizeof(interrupt_controller_t));
    interrupt_init(ic_ptr);
    ic = ic_ptr;
}

CPU6502::~CPU6502() {
    if (ic) free(static_cast<interrupt_controller_t*>(ic));
}

int CPU6502::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, &dt, CPU_6502);
    return (int)(cycles - start_cycles);
}

CPU6502Undocumented::CPU6502Undocumented() {
    memset(&dt, 0, sizeof(dt));
    dispatch_build(&dt, opcodes_6502, OPCODES_6502_COUNT, CPU_6502_UNDOCUMENTED);
    dispatch_build(&dt, opcodes_6502_undoc, OPCODES_6502_UNDOC_COUNT, CPU_6502_UNDOCUMENTED);
}

void CPU6502::trigger_interrupt(int vector_addr) {
    /* Basic 6502 interrupt logic: push PC, push P, jump to vector */
    mem_write(mem, 0x100 + s, (pc >> 8) & 0xFF);
    s--;
    mem_write(mem, 0x100 + s, pc & 0xFF);
    s--;
    mem_write(mem, 0x100 + s, p | FLAG_U);
    s--;
    set_flag(FLAG_I, 1);
    pc = mem_read(mem, vector_addr) | (mem_read(mem, vector_addr + 1) << 8);
    cycles += 7;
}

CPU65C02::CPU65C02() {
    memset(&dt, 0, sizeof(dt));
    dispatch_build(&dt, opcodes_6502, OPCODES_6502_COUNT, CPU_65C02);
    dispatch_build(&dt, opcodes_65c02, OPCODES_65C02_COUNT, CPU_65C02);
}

CPU65CE02::CPU65CE02() {
    memset(&dt, 0, sizeof(dt));
    dispatch_build(&dt, opcodes_6502, OPCODES_6502_COUNT, CPU_65CE02);
    dispatch_build(&dt, opcodes_65c02, OPCODES_65C02_COUNT, CPU_65CE02);
    dispatch_build(&dt, opcodes_65ce02, OPCODES_65CE02_COUNT, CPU_65CE02);
}

CPU45GS02::CPU45GS02() {
    memset(&dt, 0, sizeof(dt));
    dispatch_build(&dt, opcodes_6502, OPCODES_6502_COUNT, CPU_45GS02);
    dispatch_build(&dt, opcodes_65c02, OPCODES_65C02_COUNT, CPU_45GS02);
    dispatch_build(&dt, opcodes_65ce02, OPCODES_65CE02_COUNT, CPU_45GS02);
    dispatch_build(&dt, opcodes_45gs02, OPCODES_45GS02_COUNT, CPU_45GS02);
}

int CPU45GS02::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, &dt, CPU_45GS02);
    return (int)(cycles - start_cycles);
}

CPU* CPUFactory::create(cpu_type_t type) {
    switch (type) {
        case CPU_6502: return new CPU6502();
        case CPU_6502_UNDOCUMENTED: return new CPU6502Undocumented();
        case CPU_65C02: return new CPU65C02();
        case CPU_65CE02: return new CPU65CE02();
        case CPU_45GS02: return new CPU45GS02();
        default: return new CPU6502();
    }
}
