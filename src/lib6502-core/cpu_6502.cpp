#include "cpu_6502.h"
#include <stdlib.h>
#include "cpu_engine.h"
#include "opcodes/opcodes.h"
#include "interrupts.h"
#include <string.h>

CPU6502::CPU6502() {
    dt = std::make_unique<dispatch_table_t>();
    for (int i = 0; i < 256; i++) {
        dt->base[i].fn = nullptr;
        dt->eom[i].fn = nullptr;
        dt->quad[i].fn = nullptr;
        dt->quad_eom[i].fn = nullptr;
    }
    dispatch_build(dt.get(), opcodes_6502, OPCODES_6502_COUNT, CPU_6502);
    interrupt_controller_t* ic_ptr = new interrupt_controller_t();
    interrupt_init(ic_ptr);
    ic = ic_ptr;
}

CPU6502::~CPU6502() {
    if (ic) delete static_cast<interrupt_controller_t*>(ic);
}

int CPU6502::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, dt.get(), CPU_6502);
    return (int)(cycles - start_cycles);
}

CPU6502Undocumented::CPU6502Undocumented() {
    memset(dt.get(), 0, sizeof(dispatch_table_t));
    dispatch_build(dt.get(), opcodes_6502, OPCODES_6502_COUNT, CPU_6502_UNDOCUMENTED);
    dispatch_build(dt.get(), opcodes_6502_undoc, OPCODES_6502_UNDOC_COUNT, CPU_6502_UNDOCUMENTED);
}

int CPU6502Undocumented::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, dt.get(), CPU_6502_UNDOCUMENTED);
    return (int)(cycles - start_cycles);
}

void CPU6502::trigger_interrupt(int vector_addr) {
    /* Basic 6502 interrupt logic: push PC, push P, jump to vector */
    mem_write(mem, (uint16_t)(0x100 + s), (pc >> 8) & 0xFF);
    s--;
    mem_write(mem, (uint16_t)(0x100 + s), pc & 0xFF);
    s--;
    mem_write(mem, (uint16_t)(0x100 + s), (p & ~FLAG_B) | FLAG_U);  /* B=0 for hardware IRQ/NMI */
    s--;
    set_flag(FLAG_I, 1);
    pc = mem_read(mem, (uint16_t)vector_addr) | (mem_read(mem, (uint16_t)(vector_addr + 1)) << 8);
    cycles += 7;
}

CPU65C02::CPU65C02() {
    memset(dt.get(), 0, sizeof(dispatch_table_t));
    dispatch_build(dt.get(), opcodes_6502, OPCODES_6502_COUNT, CPU_65C02);
    dispatch_build(dt.get(), opcodes_65c02, OPCODES_65C02_COUNT, CPU_65C02);
}

int CPU65C02::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, dt.get(), CPU_65C02);
    return (int)(cycles - start_cycles);
}

CPU65CE02::CPU65CE02() {
    memset(dt.get(), 0, sizeof(dispatch_table_t));
    dispatch_build(dt.get(), opcodes_6502, OPCODES_6502_COUNT, CPU_65CE02);
    dispatch_build(dt.get(), opcodes_65c02, OPCODES_65C02_COUNT, CPU_65CE02);
    dispatch_build(dt.get(), opcodes_65ce02, OPCODES_65CE02_COUNT, CPU_65CE02);
}

int CPU65CE02::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, dt.get(), CPU_65CE02);
    return (int)(cycles - start_cycles);
}

CPU45GS02::CPU45GS02() {
    memset(dt.get(), 0, sizeof(dispatch_table_t));
    dispatch_build(dt.get(), opcodes_6502, OPCODES_6502_COUNT, CPU_45GS02);
    dispatch_build(dt.get(), opcodes_65c02, OPCODES_65C02_COUNT, CPU_45GS02);
    dispatch_build(dt.get(), opcodes_65ce02, OPCODES_65CE02_COUNT, CPU_45GS02);
    dispatch_build(dt.get(), opcodes_45gs02, OPCODES_45GS02_COUNT, CPU_45GS02);
}

int CPU45GS02::step() {
    unsigned long start_cycles = cycles;
    execute_from_mem(this, mem, dt.get(), CPU_45GS02);
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
