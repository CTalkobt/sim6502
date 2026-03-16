#include "../commands.h"
#include "FinishCmd.h"
#include "cpu_engine.h"
#include <stdio.h>
#include <string.h>

extern int handle_trap_local(const symbol_table_t *st, cpu_t *cpu, memory_t *mem);
extern void cli_hist_push(const CPUState *pre, const memory_t *mem);
extern void json_exec_result(const char *cmd, const char *stop_reason, const cpu_t *cpu);
extern int g_json_mode;

bool FinishCmd::execute(const std::vector<std::string>& args,
                        CPU *cpu, memory_t *mem,
                        cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                        breakpoint_list_t *breakpoints,
                        symbol_table_t *symbols) {
    (void)args; (void)breakpoints; (void)p_cpu_type; (void)dt; (void)symbols;
    
    // For 6502/65C02/65CE02, stack is at $0100 + S.
    // The CPU object handles its own S width.
    // RTS pulls lo then hi.
    uint16_t s = cpu->s;
    uint8_t lo = mem_read(mem, (uint16_t)(0x0100 + ((s + 1) & 0xFF)));
    uint8_t hi = mem_read(mem, (uint16_t)(0x0100 + ((s + 2) & 0xFF)));
    uint16_t return_addr = (uint16_t)((hi << 8) | lo) + 1;
    
    if (return_addr == 1) {
        if (g_json_mode) json_exec_result("finish", "error", cpu);
        else cli_printf("Error: Stack looks empty, cannot finish.\n");
        return true;
    }
    
    const char *stop_reason = "brk";
    while (1) {
        if (cpu->pc == return_addr) { stop_reason = "step"; break; }
        
        mem->mem_writes = 0;
        int tr = handle_trap_local(symbols, cpu, mem);
        if (tr < 0) { stop_reason = "trap"; break; }
        if (tr > 0) continue;
        
        unsigned char opc = mem_read(mem, cpu->pc);
        if (opc == 0x00) { stop_reason = "brk"; break; }
        
        if (breakpoint_hit(breakpoints, cpu)) { stop_reason = "breakpoint"; break; }
        
        CPUState pre = *cpu;
        cpu->step();
        if (mem->io_registry) mem->io_registry->tick_all(cpu->cycles);
        cli_hist_push(&pre, mem);
    }
    
    if (g_json_mode) json_exec_result("finish", stop_reason, cpu);
    else cli_printf("STOP at $%04X (%s)\n", cpu->pc, stop_reason);
    
    return true;
}
