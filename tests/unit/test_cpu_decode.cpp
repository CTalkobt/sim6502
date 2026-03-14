#include "catch.hpp"
#include "cpu_6502.h"
#include "cpu_engine.h"
#include <cstring>

TEST_CASE("Addressing Mode Decoding", "[cpu][decode]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_6502);
    memset(&mem, 0, sizeof(mem));
    cpu->mem = &mem;
    cpu->reset();
    
    SECTION("Immediate Mode") {
        cpu->pc = 0x1000;
        mem.mem[0x1001] = 0x42;
        unsigned short arg = decode_operand(cpu, &mem, MODE_IMMEDIATE);
        CHECK(arg == 0x42);
    }
    
    SECTION("Absolute Mode") {
        cpu->pc = 0x1000;
        mem.mem[0x1001] = 0x34;
        mem.mem[0x1002] = 0x12;
        unsigned short arg = decode_operand(cpu, &mem, MODE_ABSOLUTE);
        CHECK(arg == 0x1234);
    }

    SECTION("Zero Page Mode") {
        cpu->pc = 0x1000;
        mem.mem[0x1001] = 0x80;
        unsigned short arg = decode_operand(cpu, &mem, MODE_ZP);
        CHECK(arg == 0x80);
    }

    SECTION("Indirect Mode") {
        cpu->pc = 0x1000;
        mem.mem[0x1001] = 0x00;
        mem.mem[0x1002] = 0x20;
        unsigned short arg = decode_operand(cpu, &mem, MODE_INDIRECT);
        CHECK(arg == 0x2000);
    }

    delete cpu;
}

TEST_CASE("45GS02 Specific Addressing Modes", "[cpu][decode][45gs02]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_45GS02);
    memset(&mem, 0, sizeof(mem));
    cpu->mem = &mem;
    cpu->reset();

    SECTION("Immediate Word") {
        cpu->pc = 0x1000;
        mem.mem[0x1001] = 0x34;
        mem.mem[0x1002] = 0x12;
        unsigned short arg = decode_operand(cpu, &mem, MODE_IMMEDIATE_WORD);
        CHECK(arg == 0x1234);
    }

    delete cpu;
}
