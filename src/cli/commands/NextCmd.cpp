#include "NextCmd.h"
#include "cpu_engine.h"
#include <stdio.h>
#include <string.h>

extern int handle_trap_local(const symbol_table_t *st, cpu_t *cpu, memory_t *mem);
extern void cli_hist_push(const CPUState *pre, const memory_t *mem);
extern void json_exec_result(const char *cmd, const char *stop_reason, const cpu_t *cpu);
extern int g_json_mode;

bool NextCmd::execute(const std::vector<std::string>& args,
                      CPU *cpu, memory_t *mem,
                      cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                      breakpoint_list_t *breakpoints,
                      symbol_table_t *symbols) {
    (void)args; (void)breakpoints;
    
    unsigned char opc = mem_read(mem, cpu->pc);
    const dispatch_entry_t *te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
    
    bool is_jsr = (opc == 0x20); // Standard JSR abs
    if (!is_jsr && te && te->mnemonic) {
        // Also check for 65CE02/45GS02 JSR variants
        if (strcmp(te->mnemonic, "JSR") == 0 || strcmp(te->mnemonic, "BSR") == 0) {
            is_jsr = true;
        }
    }
    
    if (is_jsr && te) {
        char dummy[128];
        int instr_bytes = disasm_one(mem, dt, *p_cpu_type, cpu->pc, dummy, sizeof(dummy));
        uint16_t next_pc = (uint16_t)(cpu->pc + instr_bytes);
        // Set temporary breakpoint at next_pc
        breakpoint_add(breakpoints, next_pc, NULL);
        
        const char *stop_reason = "brk";
        while (1) {
            mem->mem_writes = 0;
            int tr = handle_trap_local(symbols, cpu, mem);
            if (tr < 0) { stop_reason = "trap"; break; }
            if (tr > 0) continue;
            
            unsigned char cur_opc = mem_read(mem, cpu->pc);
            if (cur_opc == 0x00) { stop_reason = "brk"; break; }
            
            const dispatch_entry_t *cur_te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
            if (cur_te && cur_te->mnemonic && strcmp(cur_te->mnemonic, "STP") == 0) { stop_reason = "stp"; break; }
            
            if (cpu->pc == next_pc) { stop_reason = "step"; break; }
            if (breakpoint_hit(breakpoints, cpu) && cpu->pc != next_pc) { stop_reason = "breakpoint"; break; }
            
            CPUState pre = *cpu;
            cpu->step();
            if (mem->io_registry) mem->io_registry->tick_all(cpu->cycles);
            cli_hist_push(&pre, mem);
        }
        
        breakpoint_remove(breakpoints, next_pc);
        
        if (g_json_mode) json_exec_result("next", stop_reason, cpu);
        else printf("STOP at $%04X (%s)\n", cpu->pc, stop_reason);
    } else {
        // Normal step
        mem->mem_writes = 0;
        int tr = handle_trap_local(symbols, cpu, mem);
        if (tr < 0) { 
            if (g_json_mode) json_exec_result("next", "trap", cpu);
            else printf("STOP at $%04X (trap)\n", cpu->pc);
            return true;
        }
        
        CPUState pre = *cpu;
        cpu->step();
        if (mem->io_registry) mem->io_registry->tick_all(cpu->cycles);
        cli_hist_push(&pre, mem);
        
        if (g_json_mode) json_exec_result("next", "step", cpu);
        else printf("STOP at $%04X\n", cpu->pc);
    }
    
    return true;
}
