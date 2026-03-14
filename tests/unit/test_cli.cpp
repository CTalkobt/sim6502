#include "catch.hpp"
#include "CommandRegistry.h"
#include "StepCmd.h"
#include "BreakCmd.h"
#include "cpu_6502.h"
#include <cstring>

/* Mock symbols for CLI commands - using C++ linkage to match StepCmd.cpp etc. */
int g_json_mode = 0;
int s_snap_active = 0;

int handle_trap_local(const symbol_table_t *st, CPU *cpu, memory_t *mem) { (void)st; (void)cpu; (void)mem; return 0; }
void cli_hist_push(const CPUState *pre, const memory_t *mem) { (void)pre; (void)mem; }
void json_exec_result(const char *cmd, const char *stop_reason, const CPU *cpu) { (void)cmd; (void)stop_reason; (void)cpu; }
void cli_snap_record(uint16_t addr, uint8_t before, uint8_t after, uint16_t writer_pc) { (void)addr; (void)before; (void)after; (void)writer_pc; }
void json_err(const char *cmd, const char *msg) { (void)cmd; (void)msg; }
void json_ok(const char *cmd) { (void)cmd; }

// Missing from previous run:
int cli_hist_step_back(CPU *cpu, memory_t *mem) { (void)cpu; (void)mem; return 0; }
int cli_hist_step_fwd(CPU *cpu, memory_t *mem, dispatch_table_t *dt, cpu_type_t cpu_type) { (void)cpu; (void)mem; (void)dt; (void)cpu_type; return 0; }

TEST_CASE("CLI - Command Coverage", "[cli][registry]") {
    CommandRegistry registry;
    
    // Core Navigation & Execution
    CHECK(registry.getCommand("step") != nullptr);
    CHECK(registry.getCommand("next") != nullptr);
    CHECK(registry.getCommand("finish") != nullptr);
    CHECK(registry.getCommand("history") != nullptr);
    CHECK(registry.getCommand("sb") != nullptr);
    CHECK(registry.getCommand("sf") != nullptr);
    
    // Debugging
    CHECK(registry.getCommand("break") != nullptr);
    
    // System & Environment
    CHECK(registry.getCommand("env") != nullptr);
    CHECK(registry.getCommand("devices") != nullptr);
    
    // Idioms / Patterns
    CHECK(registry.getCommand("idioms") != nullptr);
    CHECK(registry.getCommand("list_patterns") != nullptr);
    CHECK(registry.getCommand("get_pattern") != nullptr);
}

TEST_CASE("CLI - Step Command Logic", "[cli][step]") {
    StepCmd cmd;
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    CPU* cpu = CPUFactory::create(CPU_6502);
    cpu->mem = &mem;
    cpu->reset();
    
    cpu_type_t cpu_type = CPU_6502;
    
    mem.mem[0] = 0xA9; // LDA #$42
    mem.mem[1] = 0x42;
    cpu->pc = 0;
    
    SECTION("Single step") {
        std::vector<std::string> args = {"step"};
        cmd.execute(args, cpu, &mem, &cpu_type, cpu->dispatch_table(), nullptr, nullptr);
        
        CHECK(cpu->a == 0x42);
        CHECK(cpu->pc == 2);
    }
    
    SECTION("Multiple steps") {
        mem.mem[2] = 0xA9; // LDA #$43
        mem.mem[3] = 0x43;
        
        std::vector<std::string> args = {"step", "2"};
        cmd.execute(args, cpu, &mem, &cpu_type, cpu->dispatch_table(), nullptr, nullptr);
        
        CHECK(cpu->a == 0x43);
        CHECK(cpu->pc == 4);
    }
    
    delete cpu;
}

TEST_CASE("CLI - Break Command Logic", "[cli][break]") {
    BreakCmd cmd;
    breakpoint_list_t breakpoints;
    breakpoint_init(&breakpoints);
    
    SECTION("Set breakpoint") {
        std::vector<std::string> args = {"break", "$1234"};
        cmd.execute(args, nullptr, nullptr, nullptr, nullptr, &breakpoints, nullptr);
        
        CHECK(breakpoints.count == 1);
        CHECK(breakpoints.breakpoints[0].address == 0x1234);
    }
}
