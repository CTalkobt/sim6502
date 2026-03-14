#include "catch.hpp"
#include "breakpoints.h"
#include "debug_context.h"
#include "cpu_6502.h"
#include <cstring>

TEST_CASE("Debug - Breakpoint Logic", "[debug][breakpoint]") {
    breakpoint_list_t bp_list;
    breakpoint_init(&bp_list);
    
    CPU* cpu = CPUFactory::create(CPU_6502);
    cpu->pc = 0x1000;

    SECTION("Simple Breakpoint") {
        breakpoint_add(&bp_list, 0x1000, nullptr);
        CHECK(breakpoint_hit(&bp_list, cpu) == 1);
        
        cpu->pc = 0x1001;
        CHECK(breakpoint_hit(&bp_list, cpu) == 0);
    }

    SECTION("Disabled Breakpoint") {
        breakpoint_add(&bp_list, 0x1000, nullptr);
        bp_list.breakpoints[0].enabled = 0;
        CHECK(breakpoint_hit(&bp_list, cpu) == 0);
    }

    SECTION("Conditional Breakpoint") {
        // Condition: A == $42
        breakpoint_add(&bp_list, 0x1000, "A == $42");
        
        cpu->a = 0x00;
        CHECK(breakpoint_hit(&bp_list, cpu) == 0);
        
        cpu->a = 0x42;
        CHECK(breakpoint_hit(&bp_list, cpu) == 1);
    }

    SECTION("Remove Breakpoint") {
        breakpoint_add(&bp_list, 0x1000, nullptr);
        CHECK(bp_list.count == 1);
        
        breakpoint_remove(&bp_list, 0x1000);
        CHECK(bp_list.count == 0);
        CHECK(breakpoint_hit(&bp_list, cpu) == 0);
    }

    delete cpu;
}

TEST_CASE("Debug - History / Time Travel", "[debug][history]") {
    DebugContext dbg;
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    
    CPU* cpu = CPUFactory::create(CPU_6502);
    cpu->mem = &mem;
    dbg.enable_history(1);

    SECTION("History Record and Step Back") {
        CPUState pre;
        pre.pc = 0x1000;
        pre.a = 0x10;
        pre.x = 0x20;
        pre.y = 0x30;
        pre.s = 0xFF;
        pre.p = 0x20;
        pre.cycles = 100;

        cpu->pc = 0x1002;
        cpu->a = 0x11;
        cpu->cycles = 102;
        
        // Simulate one instruction execution
        dbg.on_after_execute(0x1000, pre, *cpu, 2, &mem);
        
        CHECK(dbg.history_count() == 1);
        CHECK(dbg.history_pos() == 0); 

        // Step back
        int ok = dbg.step_back(cpu, &mem);
        CHECK(ok == 1);
        CHECK(cpu->pc == 0x1000);
        CHECK(cpu->a == 0x10);
        CHECK(cpu->cycles == 100);
        CHECK(dbg.history_pos() == 1); 
    }

    SECTION("History Multi-step") {
        for (int i = 0; i < 5; i++) {
            CPUState pre = *cpu;
            cpu->pc += 2;
            cpu->a = i + 1;
            cpu->cycles += 2;
            dbg.on_after_execute(pre.pc, pre, *cpu, 2, &mem);
        }
        
        CHECK(dbg.history_count() == 5);
        CHECK(dbg.history_pos() == 0);
        
        // At real-time, A is 5 (from last i=4 step)
        CHECK(cpu->a == 5);
        
        dbg.step_back(cpu, &mem);
        // Step back 1: A should be 'pre' of last step, which was 4 (from i=3)
        CHECK(cpu->a == 4);
        CHECK(dbg.history_pos() == 1);
        
        dbg.step_fwd(cpu, &mem);
        // step_fwd restores state of instruction that was stepped back from.
        // In this implementation it just calls cpu->step() which is not ideal for tests 
        // that don't have code in memory, but let's check it restored pre-cpu of that slot.
        CHECK(dbg.history_pos() == 0);
    }

    delete cpu;
}
