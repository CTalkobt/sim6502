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

TEST_CASE("API - sim_break_get return value contract", "[api][breakpoint]") {
    // Root cause of the SaveSettings bug: sim_break_get returns 1 on success
    // and 0 on failure. The GUI save loop was checking == 0 (inverted), so
    // breakpoints were never written to config.
    sim_session_t *s = sim_create("6502");
    REQUIRE(s != nullptr);

    sim_break_set(s, 0x1000, nullptr);
    REQUIRE(sim_break_count(s) == 1);

    uint16_t addr = 0;
    char cond[128];
    cond[0] = '\0';

    SECTION("Valid index returns non-zero (success)") {
        int r = sim_break_get(s, 0, &addr, cond, sizeof(cond));
        CHECK(r != 0);
        CHECK(addr == 0x1000);
    }

    SECTION("Out-of-range index returns 0 (failure)") {
        CHECK(sim_break_get(s, 1,  &addr, cond, sizeof(cond)) == 0);
        CHECK(sim_break_get(s, -1, &addr, cond, sizeof(cond)) == 0);
    }

    sim_destroy(s);
}

TEST_CASE("API - sim_step_over preserves user breakpoint at next_pc", "[api][step_over][breakpoint]") {
    // Bug fix: step_over called sim_break_clear(next_pc) unconditionally, which
    // deleted a pre-existing user breakpoint at that address.
    sim_session_t *s = sim_create("6502");
    REQUIRE(s != nullptr);

    // 0x1000: JSR $1006  (3 bytes → next_pc = 0x1003)
    sim_mem_write_byte(s, 0x1000, 0x20);
    sim_mem_write_byte(s, 0x1001, 0x06);
    sim_mem_write_byte(s, 0x1002, 0x10);
    // 0x1003: NOP  (the address step_over aims for)
    sim_mem_write_byte(s, 0x1003, 0xEA);
    // 0x1006: RTS
    sim_mem_write_byte(s, 0x1006, 0x60);

    sim_set_pc(s, 0x1000);
    sim_set_reg_value(s, "S", 0xFF);
    sim_set_state(s, SIM_PAUSED);

    SECTION("User BP at next_pc survives step_over") {
        sim_break_set(s, 0x1003, nullptr);
        REQUIRE(sim_break_count(s) == 1);

        sim_step_over(s);

        CHECK(sim_break_count(s) == 1);
        CHECK(sim_has_breakpoint(s, 0x1003));
    }

    SECTION("No pre-existing BP: temp BP is removed after step_over") {
        REQUIRE(sim_break_count(s) == 0);

        sim_step_over(s);

        CHECK(sim_break_count(s) == 0);
        CHECK(!sim_has_breakpoint(s, 0x1003));
    }

    SECTION("PC lands at next_pc after step_over") {
        sim_step_over(s);
        CHECK(sim_get_cpu(s)->pc == 0x1003);
    }

    sim_destroy(s);
}

TEST_CASE("API - Toggle breakpoint", "[api][breakpoint][toggle]") {
    // Exercises the pattern used by OnToggleBreakpoint.
    sim_session_t *s = sim_create("6502");
    REQUIRE(s != nullptr);

    const uint16_t addr = 0x0400;

    CHECK(!sim_has_breakpoint(s, addr));

    sim_break_set(s, addr, nullptr);
    CHECK(sim_has_breakpoint(s, addr));
    CHECK(sim_break_count(s) == 1);

    sim_break_clear(s, addr);
    CHECK(!sim_has_breakpoint(s, addr));
    CHECK(sim_break_count(s) == 0);

    sim_destroy(s);
}

TEST_CASE("API - Reverse continue loop", "[api][history][reverse]") {
    // Exercises the step-back-until-breakpoint loop used by OnReverseContinue.
    sim_session_t *s = sim_create("6502");
    REQUIRE(s != nullptr);

    sim_history_enable(s, 1);
    sim_history_clear(s);

    // 0x2000: LDA #$01
    sim_mem_write_byte(s, 0x2000, 0xA9);
    sim_mem_write_byte(s, 0x2001, 0x01);
    // 0x2002: LDA #$02
    sim_mem_write_byte(s, 0x2002, 0xA9);
    sim_mem_write_byte(s, 0x2003, 0x02);

    sim_set_pc(s, 0x2000);
    sim_set_reg_byte(s, "A", 0x00);
    sim_set_state(s, SIM_PAUSED);

    sim_step(s, 2);
    REQUIRE(sim_get_cpu(s)->pc == 0x2004);
    REQUIRE(sim_get_cpu(s)->a  == 0x02);
    REQUIRE(sim_history_count(s) == 2);

    SECTION("Stops at breakpoint, restores pre-instruction state") {
        sim_break_set(s, 0x2002, nullptr);

        // Mirror of OnReverseContinue loop
        while (sim_history_step_back(s)) {
            const CPU *cpu = sim_get_cpu(s);
            if (cpu && sim_has_breakpoint(s, cpu->pc))
                break;
        }

        const CPU *cpu = sim_get_cpu(s);
        CHECK(cpu->pc == 0x2002);
        CHECK(cpu->a  == 0x01); // A before LDA #$02 ran
    }

    SECTION("Exhausts history cleanly when no breakpoint set") {
        int steps = 0;
        while (sim_history_step_back(s))
            steps++;

        CHECK(steps == 2);
        CHECK(sim_get_cpu(s)->pc == 0x2000); // rewound to start
        CHECK(sim_get_cpu(s)->a  == 0x00);
    }

    SECTION("Does nothing when history was never enabled (null buffer)") {
        // step_back guards on hist_buf_ == nullptr; a session that never had
        // history enabled has no buffer, so step_back returns 0 immediately.
        sim_session_t *fresh = sim_create("6502");
        REQUIRE(fresh != nullptr);
        int steps = 0;
        while (sim_history_step_back(fresh))
            steps++;
        CHECK(steps == 0);
        sim_destroy(fresh);
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
    sim_set_reg_value(s, "S", 0x01FE); 
    sim_set_state(s, SIM_PAUSED);

    SECTION("Stop on Breakpoint") {
        sim_break_set(s, 0x1002, nullptr);
        
        // Step 2 instructions
        int event = sim_step(s, 2);
        CHECK(event == SIM_EVENT_BREAK);
        
        CPU* cpu = sim_get_cpu(s);
        CHECK(cpu->pc == 0x1002);
        CHECK(cpu->a == 0x01);
        CHECK(cpu->s == 0x01FE);
    }

    SECTION("Set other registers") {
        sim_set_reg_value(s, "A", 0x123);
        sim_set_reg_value(s, "X", 0x456);
        CPU* cpu = sim_get_cpu(s);
        CHECK(cpu->a == 0x23);
        CHECK(cpu->x == 0x56);
    }

    SECTION("History State Diff") {
        sim_history_enable(s, 1);
        sim_history_clear(s);
        
        // LDA #$10
        sim_mem_write_byte(s, 0x2000, 0xA9);
        sim_mem_write_byte(s, 0x2001, 0x10);
        sim_set_pc(s, 0x2000);
        sim_set_reg_byte(s, "A", 0x00);
        sim_set_state(s, SIM_PAUSED);
        
        // Step once
        sim_step(s, 1);
        
        CPU* cpu = sim_get_cpu(s);
        CHECK(cpu->a == 0x10);
        CHECK(cpu->pc == 0x2002);
        
        // Check history for PREV state
        sim_history_entry_t entry;
        int ok = sim_history_get(s, 0, &entry);
        REQUIRE(ok == 1);
        
        // entry.pre_cpu is the state BEFORE execution of that slot
        // We want to ensure history contains a different 'A'
        CHECK(entry.pre_cpu.a == 0x00);
        CHECK(cpu->a == 0x10);
        CHECK(entry.pc == 0x2000);
    }

    sim_destroy(s);
}
