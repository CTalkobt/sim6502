#include "../commands.h"
#include "StepCmd.h"
#include "cpu_engine.h"
#include "condition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for functions still in commands.cpp for now */
extern int handle_trap_local(const symbol_table_t *st, cpu_t *cpu, memory_t *mem);
extern void cli_hist_push(const CPUState *pre, const memory_t *mem);
extern void json_exec_result(const char *cmd, const char *stop_reason, const cpu_t *cpu);
extern int g_json_mode;
extern int s_snap_active;
extern void cli_snap_record(uint16_t addr, uint8_t before, uint8_t after, uint16_t writer_pc);

bool StepCmd::execute(const std::vector<std::string>& args,
                      CPU *cpu, memory_t *mem,
                      cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                      breakpoint_list_t *breakpoints,
                      symbol_table_t *symbols) {
    (void)breakpoints;
    int steps = 1;
    if (args.size() > 1) {
        steps = atoi(args[1].c_str());
        if (steps < 1) steps = 1;
    }

    const char *stop_reason = "step";
    for (int i = 0; i < steps; i++) {
        mem->mem_writes = 0;
        int tr = handle_trap_local(symbols, cpu, mem);
        if (tr < 0) { stop_reason = "trap"; break; }
        if (tr > 0) continue;

        unsigned char opc = mem_read(mem, cpu->pc);
        if (opc == 0x00) { stop_reason = "brk"; break; }

        const dispatch_entry_t *te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
        if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) { stop_reason = "stp"; break; }

        CPUState pre = *cpu;
        cpu->step();
        cli_hist_push(&pre, mem);

        if (s_snap_active) {
            int _nw = mem->mem_writes < 256 ? mem->mem_writes : 256;
            for (int _d = 0; _d < _nw; _d++)
                cli_snap_record(mem->mem_addr[_d], mem->mem_old_val[_d],
                                mem->mem_val[_d], pre.pc);
        }
    }

    if (g_json_mode) json_exec_result("step", stop_reason, cpu);
    else cli_printf("STOP $%04X\n", cpu->pc);

    return true;
}
