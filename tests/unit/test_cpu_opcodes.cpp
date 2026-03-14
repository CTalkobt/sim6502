#include "catch.hpp"
#include "cpu_6502.h"
#include <cstring>

static void setup_cpu(CPU* cpu, memory_t* mem) {
    memset(mem, 0, sizeof(memory_t));
    cpu->mem = mem;
    cpu->reset();
}

TEST_CASE("CPU Opcode - BIT", "[cpu][bit]") {
    memory_t mem;
    
    SECTION("BIT 6502 ZP") {
        CPU* cpu = CPUFactory::create(CPU_6502);
        setup_cpu(cpu, &mem);
        
        // $24 = BIT ZP
        mem.mem[0x1000] = 0x24;
        mem.mem[0x1001] = 0x10;
        mem.mem[0x0010] = 0xC0; // N=1, V=1
        cpu->pc = 0x1000;
        cpu->a = 0xFF;
        
        cpu->step();
        CHECK(cpu->get_flag(FLAG_N) == 1);
        CHECK(cpu->get_flag(FLAG_V) == 1);
        CHECK(cpu->get_flag(FLAG_Z) == 0);
        delete cpu;
    }

    SECTION("BIT 65C02 Immediate") {
        CPU* cpu = CPUFactory::create(CPU_65C02);
        setup_cpu(cpu, &mem);
        
        // $89 = BIT Imm
        mem.mem[0x1000] = 0x89;
        mem.mem[0x1001] = 0x01; // Only Z flag is affected by Imm BIT on 65C02
        cpu->pc = 0x1000;
        cpu->a = 0xFE;
        
        cpu->step();
        CHECK(cpu->get_flag(FLAG_Z) == 1);
        // On 65C02 immediate BIT, N and V are NOT affected
        CHECK(cpu->get_flag(FLAG_N) == 0);
        CHECK(cpu->get_flag(FLAG_V) == 0);
        delete cpu;
    }
}

TEST_CASE("CPU Opcode - TRB/TSB", "[cpu][bit]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_65C02);
    setup_cpu(cpu, &mem);

    SECTION("TSB ZP") {
        // $04 = TSB ZP
        mem.mem[0x1000] = 0x04;
        mem.mem[0x1001] = 0x10;
        mem.mem[0x0010] = 0x01;
        cpu->pc = 0x1000;
        cpu->a = 0x02;
        
        cpu->step();
        CHECK(mem.mem[0x0010] == 0x03); // 0x01 | 0x02
        CHECK(cpu->get_flag(FLAG_Z) == 1); // Z is set based on A & old_mem
        delete cpu;
    }
}

TEST_CASE("CPU Opcode - Branching", "[cpu][branch]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_6502);
    setup_cpu(cpu, &mem);

    SECTION("BNE Taken") {
        // $D0 = BNE
        mem.mem[0x1000] = 0xD0;
        mem.mem[0x1001] = 0x05; // +5
        cpu->pc = 0x1000;
        cpu->set_flag(FLAG_Z, 0);
        
        cpu->step();
        CHECK(cpu->pc == 0x1007); // 1002 + 5
        delete cpu;
    }

    SECTION("BNE Not Taken") {
        mem.mem[0x1000] = 0xD0;
        mem.mem[0x1001] = 0x05;
        cpu->pc = 0x1000;
        cpu->set_flag(FLAG_Z, 1);
        
        cpu->step();
        CHECK(cpu->pc == 0x1002);
        delete cpu;
    }

    SECTION("BNE Negative displacement") {
        mem.mem[0x1000] = 0xD0;
        mem.mem[0x1001] = 0xFB; // -5
        cpu->pc = 0x1000;
        cpu->set_flag(FLAG_Z, 0);
        
        cpu->step();
        CHECK(cpu->pc == 0x1002 - 5);
        delete cpu;
    }
}

TEST_CASE("CPU Opcode - Stack Operations", "[cpu][stack]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_6502);
    setup_cpu(cpu, &mem);

    SECTION("PHA / PLA") {
        mem.mem[0x1000] = 0x48; // PHA
        mem.mem[0x1001] = 0x68; // PLA
        cpu->pc = 0x1000;
        cpu->a = 0x42;
        cpu->s = 0xFF;
        
        cpu->step();
        CHECK(cpu->s == 0xFE);
        CHECK(mem.mem[0x1FF] == 0x42);
        
        cpu->a = 0;
        cpu->step();
        CHECK(cpu->s == 0xFF);
        CHECK(cpu->a == 0x42);
        delete cpu;
    }
}
