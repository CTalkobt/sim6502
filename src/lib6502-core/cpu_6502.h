#ifndef CPU_6502_H
#define CPU_6502_H

#include "cpu.h"
#include "memory.h"
#include "disassembler.h"

class CPU6502 : public CPU {
protected:
    dispatch_table_t dt;
    void* ic;
public:
    CPU6502();
    virtual ~CPU6502();
    virtual int step() override;
    virtual void trigger_interrupt(int vector_addr) override;
    virtual void* get_interrupt_controller() override { return ic; }
    virtual dispatch_table_t* dispatch_table() override { return &dt; }
};

class CPU6502Undocumented : public CPU6502 {
public:
    CPU6502Undocumented();
};

class CPU65C02 : public CPU6502 {
public:
    CPU65C02();
};

class CPU65CE02 : public CPU65C02 {
public:
    CPU65CE02();
};

class CPU45GS02 : public CPU65CE02 {
public:
    CPU45GS02();
    virtual int step() override;
};

class CPUFactory {
public:
    static CPU* create(cpu_type_t type);
};

#endif
