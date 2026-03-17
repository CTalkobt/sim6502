#include "HelpCmd.h"
#include "CommandRegistry.h"
#include "../commands.h"
#include <string.h>

bool HelpCmd::execute(const std::vector<std::string>& args,
                      CPU *cpu, memory_t *mem,
                      cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                      breakpoint_list_t *breakpoints,
                      symbol_table_t *symbols) {
    (void)cpu; (void)mem; (void)p_cpu_type; (void)dt; (void)breakpoints; (void)symbols;

    if (args.size() > 1) {
        print_detailed_help(args[1].c_str());
    } else {
        cli_printf("Commands: step [n], run, stepback (sb), stepfwd (sf),\n"
               "          break <addr>, clear <addr>, list, regs,\n"
               "          mem <addr> [len], write <addr> <val>, reset,\n"
               "          processors, processor <type>, info <opcode>,\n"
               "          jump <addr>, set <reg> <val>, flag <flag> <0|1>,\n"
               "          bload \"file\" [addr], bsave \"file\" <start> <end>,\n"
               "          asm [addr], disasm [addr [count]],\n"
               "          vic2.info, vic2.regs, vic2.sprites,\n"
               "          sid.info, sid.regs,\n"
               "          vic2.savescreen [file], vic2.savebitmap [file],\n"
               "          validate <addr> [A=v X=v ...] : [A=v X=v ...]\n"
               "          snapshot, diff, speed [scale]%s\n"
               "Type 'help <command>' for more info.\n",
               cliIsInteractiveMode() ? ", quit" : "");
    }
    return true;
}
