#ifndef CPU_6502_H
#define CPU_6502_H

#include "cpu.h"
#include "memory.h"
#include "disassembler.h"

#include <memory>

class CPU6502 : public CPU {
protected:
    std::unique_ptr<dispatch_table_t> dt;
    void* ic;
public:
    CPU6502();
    virtual ~CPU6502();
    virtual int step() override;
    virtual void trigger_interrupt(int vector_addr) override;
    virtual void* get_interrupt_controller() override { return ic; }
    virtual dispatch_table_t* dispatch_table() override { return dt.get(); }
};

class CPU6502Undocumented : public CPU6502 {
public:
    CPU6502Undocumented();
    virtual int step() override;
};

class CPU65C02 : public CPU6502 {
public:
    CPU65C02();
    virtual int step() override;
};

class CPU65CE02 : public CPU65C02 {
public:
    CPU65CE02();
    virtual int step() override;
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
