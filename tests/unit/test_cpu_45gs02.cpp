#include "catch.hpp"
#include "cpu_6502.h"
#include <cstring>
#include <cstdlib>

static void setup_cpu(CPU* cpu, memory_t* mem) {
    memset(mem, 0, sizeof(memory_t));
    cpu->mem = mem;
    cpu->reset();
}

TEST_CASE("45GS02 - NEG instruction", "[cpu][45gs02]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_45GS02);
    setup_cpu(cpu, &mem);
    
    // $42 = NEG
    mem.mem[0x1000] = 0x42;
    cpu->pc = 0x1000;
    
    SECTION("NEG positive") {
        cpu->a = 0x01;
        cpu->step();
        CHECK(cpu->a == 0xFF); // -1
        CHECK(cpu->get_flag(FLAG_N) == 1);
        CHECK(cpu->get_flag(FLAG_Z) == 0);
    }
    
    SECTION("NEG zero") {
        cpu->a = 0x00;
        cpu->step();
        CHECK(cpu->a == 0x00);
        CHECK(cpu->get_flag(FLAG_N) == 0);
        CHECK(cpu->get_flag(FLAG_Z) == 1);
    }

    SECTION("NEG negative") {
        cpu->a = 0x80; // -128
        cpu->step();
        CHECK(cpu->a == 0x80); // -(-128) = 128 = 0x80
        CHECK(cpu->get_flag(FLAG_N) == 1);
        CHECK(cpu->get_flag(FLAG_Z) == 0);
    }
    
    delete cpu;
}

TEST_CASE("45GS02 - MAP instruction", "[cpu][45gs02][memory]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_45GS02);
    setup_cpu(cpu, &mem);
    
    // $5C = MAP
    mem.mem[0x1000] = 0x5C;
    cpu->pc = 0x1000;
    
    // Set up MAP: map $0000-$1FFF to physical $10000
    // A[7:0] = lo_offset[15:8] = $00
    // X[7:4] = lo_sel = $1 (bit0 set)
    // X[3:0] = lo_offset[19:16] = $1
    cpu->a = 0x00;
    cpu->x = 0x11;
    cpu->y = 0x00;
    cpu->z = 0x00;
    
    cpu->step();
    
    // Verify mapping
    // Virtual $0000 should now map to $10000
    // Physical $10000 is in far memory.
    
    unsigned char val = 0xAA;
    mem_write(&mem, 0x0000, val);
    
    // Check if it went to physical $10000
    CHECK(far_mem_read(&mem, 0x10000) == val);
    CHECK(mem_read(&mem, 0x0000) == val);
    
    // Check if it did NOT go to physical $0000
    CHECK(mem.mem[0x0000] == 0x00);
    
    mem_free_far_pages(&mem);
    delete cpu;
}

TEST_CASE("45GS02 - 16-bit Stack Pointer", "[cpu][45gs02][stack]") {
    memory_t mem;
    CPU* cpu = CPUFactory::create(CPU_45GS02);
    setup_cpu(cpu, &mem);
    
    SECTION("16-bit Stack Push/Pop") {
        cpu->set_flag(FLAG_E, 0); // Native mode
        cpu->s = 0x2000;
        
        // $F4 $34 $12 = PHW #$1234
        mem.mem[0x1000] = 0xF4;
        mem.mem[0x1001] = 0x34;
        mem.mem[0x1002] = 0x12;
        cpu->pc = 0x1000;
        
        cpu->step();
        
        CHECK(cpu->s == 0x1FFE);
        CHECK(mem.mem[0x2000] == 0x12);
        CHECK(mem.mem[0x1FFF] == 0x34);
        
        // $FB = PLZ (Pull Z from stack)
        mem.mem[0x1003] = 0xFB;
        cpu->pc = 0x1003;
        cpu->z = 0x00;
        
        cpu->step();
        CHECK(cpu->s == 0x1FFF);
        CHECK(cpu->z == 0x34);
    }

    SECTION("8-bit Emulation Stack") {
        cpu->set_flag(FLAG_E, 1); // Emulation mode
        cpu->s = 0xFF; // SP=$FF (page 1)
        
        // $48 = PHA
        mem.mem[0x1000] = 0x48;
        cpu->pc = 0x1000;
        cpu->a = 0xAA;
        
        cpu->step();
        CHECK(cpu->s == 0xFE);
        CHECK(mem.mem[0x01FF] == 0xAA);
    }
    
    delete cpu;
}
