#include "catch.hpp"
#include "sim_api.h"
#include <cstdio>
#include <cstring>

TEST_CASE("API - Session Lifecycle", "[api][lifecycle]") {
    sim_session_t* s = sim_create("6502");
    REQUIRE(s != nullptr);
    CHECK(sim_get_state(s) == SIM_IDLE);
    
    SECTION("Reset State") {
        sim_reset(s);
        CHECK(sim_get_state(s) == SIM_IDLE);
    }
    
    SECTION("Processor Selection") {
        CHECK(strcmp(sim_processor_name(s), "6502") == 0);
        sim_set_processor(s, "65c02");
        CHECK(strcmp(sim_processor_name(s), "65C02") == 0);
    }

    sim_destroy(s);
}

TEST_CASE("API - Execution Control", "[api][execution]") {
    sim_session_t* s = sim_create("6502");
    REQUIRE(s != nullptr);

    SECTION("Step through memory") {
        // LDA #$42
        sim_mem_write_byte(s, 0x1000, 0xA9);
        sim_mem_write_byte(s, 0x1001, 0x42);
        sim_set_pc(s, 0x1000);
        sim_set_state(s, SIM_PAUSED); // Ensure state allows stepping
        
        int event = sim_step(s, 1);
        CHECK(event == 0);
        
        CPU* cpu = sim_get_cpu(s);
        CHECK(cpu->a == 0x42);
        CHECK(cpu->pc == 0x1002);
    }

    SECTION("Load BIN") {
        const char* bin_file = "test_api.bin";
        FILE* f = fopen(bin_file, "wb");
        // Data: LDA #$AA, RTS
        fputc(0xA9, f);
        fputc(0xAA, f);
        fputc(0x60, f);
        fclose(f);
        
        int ok = sim_load_bin(s, bin_file, 0x1000);
        CHECK(ok == 0);
        CHECK(sim_get_state(s) == SIM_READY);
        
        CPU* cpu = sim_get_cpu(s);
        CHECK(cpu->pc == 0x1000);
        
        sim_step(s, 1);
        CHECK(cpu->a == 0xAA);
        
        remove(bin_file);
    }

    sim_destroy(s);
}

TEST_CASE("API - Breakpoint API", "[api][breakpoint]") {
    sim_session_t* s = sim_create("6502");
    REQUIRE(s != nullptr);

    sim_mem_write_byte(s, 0x1000, 0xA9); // LDA #$01
    sim_mem_write_byte(s, 0x1001, 0x01);
    sim_mem_write_byte(s, 0x1002, 0xA9); // LDA #$02
    sim_mem_write_byte(s, 0x1003, 0x02);
    sim_set_pc(s, 0x1000);
    sim_set_reg_byte(s, "S", 0xFE); 
    sim_set_state(s, SIM_PAUSED);

    SECTION("Stop on Breakpoint") {
        sim_break_set(s, 0x1002, nullptr);
        
        // Step 2 instructions
        int event = sim_step(s, 2);
        CHECK(event == SIM_EVENT_BREAK);
        
        CPU* cpu = sim_get_cpu(s);
        CHECK(cpu->pc == 0x1002);
        CHECK(cpu->a == 0x01);
    }

    sim_destroy(s);
}
