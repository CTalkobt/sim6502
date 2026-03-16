#include "../commands.h"
#include "HistoryCmd.h"
#include "cpu_engine.h"
#include <stdio.h>

extern int cli_hist_step_back(cpu_t *cpu, memory_t *mem);
extern int cli_hist_step_fwd(cpu_t *cpu, memory_t *mem, dispatch_table_t *dt, cpu_type_t cpu_type);

bool HistoryCmd::execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) {
    (void)breakpoints; (void)symbols;
    if (args.empty()) return true;
    
    if (args[0] == "sb" || args[0] == "stepback") {
        if (cli_hist_step_back(cpu, mem)) {
            cli_printf("BACK  PC=%04X\n", cpu->pc);
        } else {
            cli_printf("No history to step back into.\n");
        }
    } else if (args[0] == "sf" || args[0] == "stepfwd") {
        if (cli_hist_step_fwd(cpu, mem, dt, *p_cpu_type)) {
            cli_printf("FWD   PC=%04X\n", cpu->pc);
        } else {
            cli_printf("Already at the present.\n");
        }
    }
    
    return true;
}
