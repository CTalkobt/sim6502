#ifndef BREAK_CMD_H
#define BREAK_CMD_H

#include "CLICommand.h"

class BreakCmd : public CLICommand {
public:
    virtual std::string name() const override { return "break"; }
    virtual std::string help() const override { 
        return "Usage: break <addr> [condition]\n"
               "Set a breakpoint at hexadecimal address <addr>.\n"
               "Optional condition can use registers (A,X,Y,PC) and operators (==, !=, <, >, &&, ||).\n"
               "Example: break $1234 A == $42";
    }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
